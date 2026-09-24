#include "database/database_manager.h"
#include "model/business_error.h"
#include "model/charging_record.h"
#include "network/network_client.h"
#include "network/network_server_host.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>

#include <functional>

namespace {

bool waitUntil(const std::function<bool()> &condition)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1);
    }
    return condition();
}

ncs::JsonResponse findResponse(const QList<ncs::JsonResponse> &responses,
                               const QString &requestId)
{
    for (const ncs::JsonResponse &response : responses) {
        if (response.requestId == requestId) return response;
    }
    return {};
}

QJsonObject chargerFrom(const ncs::JsonResponse &detail, qint64 chargerId)
{
    for (const QJsonValue &value : detail.data.value(QStringLiteral("chargers")).toArray()) {
        const QJsonObject charger = value.toObject();
        if (charger.value(QStringLiteral("id")).toInteger() == chargerId) return charger;
    }
    return {};
}

bool forceExpired(const QString &databasePath, qint64 recordId)
{
    const QString name = QStringLiteral("expire_%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        database.setDatabaseName(databasePath);
        if (database.open()) {
            QSqlQuery query(database);
            query.prepare(QStringLiteral(
                "UPDATE charging_order SET expire_at=:expire_at WHERE id=:id"));
            query.bindValue(QStringLiteral(":expire_at"),
                            QDateTime::currentDateTimeUtc().addSecs(-1)
                                .toString(Qt::ISODateWithMs));
            query.bindValue(QStringLiteral(":id"), recordId);
            ok = query.exec() && query.numRowsAffected() == 1;
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(name);
    return ok;
}

bool orderCounts(const QString &databasePath, qint64 userId, int *total, int *reserved)
{
    const QString name = QStringLiteral("counts_%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        database.setDatabaseName(databasePath);
        if (database.open()) {
            QSqlQuery query(database);
            query.prepare(QStringLiteral(
                "SELECT COUNT(*),SUM(CASE WHEN status=0 THEN 1 ELSE 0 END) "
                "FROM charging_order WHERE user_id=:user_id"));
            query.bindValue(QStringLiteral(":user_id"), userId);
            if (query.exec() && query.next()) {
                *total = query.value(0).toInt();
                *reserved = query.value(1).toInt();
                ok = true;
            }
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(name);
    return ok;
}

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    const QString databasePath = directory.filePath(QStringLiteral("charge-entry.db"));
    {
        ncs::DatabaseManager setup(databasePath);
        if (!setup.initialize()) return 1;
        QSqlQuery query(setup.connection());
        if (!query.exec(QStringLiteral("UPDATE charger SET status=0 WHERE id IN (1,2)"))
            || !query.exec(QStringLiteral("UPDATE charger SET status=2 WHERE id=3"))) return 2;
    }

    ncs::NetworkServerHost server(databasePath);
    quint16 port = 0;
    QObject::connect(&server, &ncs::NetworkServerHost::serverStarted,
                     [&](quint16 value) { port = value; });
    server.startServer(QStringLiteral("127.0.0.1"), 0);
    if (!waitUntil([&] { return port > 0; })) return 3;

    ncs::NetworkClient clientA;
    ncs::NetworkClient clientB;
    bool connectedA = false;
    bool connectedB = false;
    QList<ncs::JsonResponse> responsesA;
    QList<ncs::JsonResponse> responsesB;
    QObject::connect(&clientA, &ncs::NetworkClient::connected, [&] { connectedA = true; });
    QObject::connect(&clientB, &ncs::NetworkClient::connected, [&] { connectedB = true; });
    QObject::connect(&clientA, &ncs::NetworkClient::responseReceived,
                     [&](const ncs::JsonResponse &response) { responsesA.append(response); });
    QObject::connect(&clientB, &ncs::NetworkClient::responseReceived,
                     [&](const ncs::JsonResponse &response) { responsesB.append(response); });
    clientA.connectToServer(QStringLiteral("127.0.0.1"), port);
    clientB.connectToServer(QStringLiteral("127.0.0.1"), port);
    if (!waitUntil([&] { return connectedA && connectedB; })) return 4;

    auto call = [&](ncs::NetworkClient &client, QList<ncs::JsonResponse> &responses,
                    const QString &id, const QString &route, const QJsonObject &data) {
        client.sendRequest(route, data, id);
        waitUntil([&] {
            return findResponse(responses, id).requestId == id;
        });
        return findResponse(responses, id);
    };
    auto login = [&](ncs::NetworkClient &client, QList<ncs::JsonResponse> &responses,
                     const QString &suffix, const QString &phone) {
        const auto otp = call(client, responses, QStringLiteral("otp-") + suffix,
                              QStringLiteral("user.otp.request"),
                              {{QStringLiteral("phone"), phone}});
        return call(client, responses, QStringLiteral("login-") + suffix,
                    QStringLiteral("user.login"),
                    {{QStringLiteral("phone"), phone},
                     {QStringLiteral("code"),
                      otp.data.value(QStringLiteral("display_code"))}});
    };
    const auto loginA = login(clientA, responsesA, QStringLiteral("a"),
                              QStringLiteral("13800138401"));
    const auto loginB = login(clientB, responsesB, QStringLiteral("b"),
                              QStringLiteral("13800138402"));
    if (!loginA.success || !loginB.success) return 5;
    const QString tokenA = loginA.data.value(QStringLiteral("session_token")).toString();
    const QString tokenB = loginB.data.value(QStringLiteral("session_token")).toString();
    const qint64 userA = loginA.data.value(QStringLiteral("user")).toObject()
                               .value(QStringLiteral("id")).toInteger();
    const auto rechargeA = call(clientA, responsesA, QStringLiteral("recharge-a"),
        QStringLiteral("user.recharge"),
        {{QStringLiteral("session_token"), tokenA}, {QStringLiteral("amount"), 10.0}});
    const auto rechargeB = call(clientB, responsesB, QStringLiteral("recharge-b"),
        QStringLiteral("user.recharge"),
        {{QStringLiteral("session_token"), tokenB}, {QStringLiteral("amount"), 10.0}});
    if (!rechargeA.success || !rechargeB.success) return 25;

    const auto initialDetail = call(clientA, responsesA, QStringLiteral("detail-initial"),
        QStringLiteral("station.detail"), {{QStringLiteral("station_id"), 1}});
    const QJsonObject idle = chargerFrom(initialDetail, 1);
    const QJsonObject fault = chargerFrom(initialDetail, 3);
    if (!initialDetail.success || idle.value(QStringLiteral("status")).toInt() != 0
        || idle.value(QStringLiteral("active_order_status")).toInt() != -1
        || fault.value(QStringLiteral("status")).toInt() != 2
        || fault.value(QStringLiteral("active_order_status")).toInt() != -1) return 6;

    // Direct charging remains a separate Idle -> Charging path with no Reserved row.
    const auto direct = call(clientA, responsesA, QStringLiteral("direct-start"),
        QStringLiteral("charge.start"),
        {{QStringLiteral("session_token"), tokenA}, {QStringLiteral("charger_id"), 1}});
    if (!direct.success
        || direct.data.value(QStringLiteral("status")).toInt()
            != static_cast<int>(ncs::ChargingOrderStatus::Charging)) return 7;
    const qint64 directId = direct.data.value(QStringLiteral("id")).toInteger();
    int totalOrders = 0;
    int reservedOrders = 0;
    if (!orderCounts(databasePath, userA, &totalOrders, &reservedOrders)
        || totalOrders != 1 || reservedOrders != 0) return 23;
    const auto activeDirect = call(clientA, responsesA, QStringLiteral("active-direct"),
        QStringLiteral("charge.active"), {{QStringLiteral("session_token"), tokenA}});
    if (!activeDirect.success
        || activeDirect.data.value(QStringLiteral("id")).toInteger() != directId) return 8;
    const auto detailCharging = call(clientB, responsesB, QStringLiteral("detail-charging"),
        QStringLiteral("station.detail"), {{QStringLiteral("station_id"), 1}});
    const QJsonObject publicCharging = chargerFrom(detailCharging, 1);
    if (publicCharging.value(QStringLiteral("active_order_status")).toInt() != 1
        || publicCharging.contains(QStringLiteral("user_id"))) return 9;
    const auto directStop = call(clientA, responsesA, QStringLiteral("direct-stop"),
        QStringLiteral("charge.settle"),
        {{QStringLiteral("session_token"), tokenA}, {QStringLiteral("order_id"), directId}});
    if (!directStop.success
        || directStop.data.value(QStringLiteral("status")).toInt()
            != static_cast<int>(ncs::ChargingOrderStatus::Completed)) return 10;

    // Reservation remains independent and is recoverable through charge.active.
    const auto reserved = call(clientA, responsesA, QStringLiteral("reserve"),
        QStringLiteral("charge.reserve"),
        {{QStringLiteral("session_token"), tokenA}, {QStringLiteral("charger_id"), 1}});
    if (!reserved.success
        || reserved.data.value(QStringLiteral("status")).toInt()
            != static_cast<int>(ncs::ChargingOrderStatus::Reserved)) return 11;
    const qint64 reservedId = reserved.data.value(QStringLiteral("id")).toInteger();
    const auto detailReserved = call(clientB, responsesB, QStringLiteral("detail-reserved"),
        QStringLiteral("station.detail"), {{QStringLiteral("station_id"), 1}});
    const QJsonObject publicReserved = chargerFrom(detailReserved, 1);
    if (publicReserved.value(QStringLiteral("active_order_status")).toInt() != 0
        || publicReserved.contains(QStringLiteral("user_id"))) return 12;
    const auto recoveredReserved = call(clientA, responsesA, QStringLiteral("active-reserved"),
        QStringLiteral("charge.active"), {{QStringLiteral("session_token"), tokenA}});
    if (!recoveredReserved.success
        || recoveredReserved.data.value(QStringLiteral("id")).toInteger() != reservedId
        || recoveredReserved.data.value(QStringLiteral("status")).toInt() != 0) return 13;

    const auto reservedBlocksUser = call(clientA, responsesA,
        QStringLiteral("reserved-blocks-user"), QStringLiteral("charge.start"),
        {{QStringLiteral("session_token"), tokenA}, {QStringLiteral("charger_id"), 2}});
    if (reservedBlocksUser.success
        || reservedBlocksUser.code
            != static_cast<int>(ncs::BusinessErrorCode::ActiveOrderExists)) return 14;
    const auto reservedBlocksOther = call(clientB, responsesB,
        QStringLiteral("reserved-blocks-other"), QStringLiteral("charge.start"),
        {{QStringLiteral("session_token"), tokenB}, {QStringLiteral("charger_id"), 1}});
    if (reservedBlocksOther.success) return 15;

    const auto startReserved = call(clientA, responsesA, QStringLiteral("start-reserved"),
        QStringLiteral("charge.start"),
        {{QStringLiteral("session_token"), tokenA},
         {QStringLiteral("order_id"), reservedId}});
    if (!startReserved.success
        || startReserved.data.value(QStringLiteral("id")).toInteger() != reservedId
        || startReserved.data.value(QStringLiteral("status")).toInt() != 1) return 16;
    if (!orderCounts(databasePath, userA, &totalOrders, &reservedOrders)
        || totalOrders != 2 || reservedOrders != 0) return 24;
    const auto recoveredCharging = call(clientA, responsesA,
        QStringLiteral("active-charging"), QStringLiteral("charge.active"),
        {{QStringLiteral("session_token"), tokenA}});
    if (!recoveredCharging.success
        || recoveredCharging.data.value(QStringLiteral("id")).toInteger() != reservedId) return 17;
    const auto chargingBlocksUser = call(clientA, responsesA,
        QStringLiteral("charging-blocks-user"), QStringLiteral("charge.start"),
        {{QStringLiteral("session_token"), tokenA}, {QStringLiteral("charger_id"), 2}});
    const auto chargingBlocksOther = call(clientB, responsesB,
        QStringLiteral("charging-blocks-other"), QStringLiteral("charge.start"),
        {{QStringLiteral("session_token"), tokenB}, {QStringLiteral("charger_id"), 1}});
    if (chargingBlocksUser.success || chargingBlocksOther.success) return 18;
    const auto stopReserved = call(clientA, responsesA, QStringLiteral("stop-reserved"),
        QStringLiteral("charge.settle"),
        {{QStringLiteral("session_token"), tokenA}, {QStringLiteral("order_id"), reservedId}});
    if (!stopReserved.success || stopReserved.data.value(QStringLiteral("status")).toInt() != 2)
        return 19;

    const auto cancelReservation = call(clientA, responsesA, QStringLiteral("reserve-cancel"),
        QStringLiteral("charge.reserve"),
        {{QStringLiteral("session_token"), tokenA}, {QStringLiteral("charger_id"), 2}});
    const qint64 cancelId = cancelReservation.data.value(QStringLiteral("id")).toInteger();
    const auto cancelled = call(clientA, responsesA, QStringLiteral("cancel"),
        QStringLiteral("charge.cancel"),
        {{QStringLiteral("session_token"), tokenA}, {QStringLiteral("order_id"), cancelId}});
    const auto detailCancelled = call(clientA, responsesA, QStringLiteral("detail-cancelled"),
        QStringLiteral("station.detail"), {{QStringLiteral("station_id"), 1}});
    const QJsonObject released = chargerFrom(detailCancelled, 2);
    if (!cancelReservation.success || !cancelled.success
        || cancelled.data.value(QStringLiteral("status")).toInt() != 3
        || released.value(QStringLiteral("status")).toInt() != 0
        || released.value(QStringLiteral("active_order_status")).toInt() != -1) return 20;

    const auto expiring = call(clientA, responsesA, QStringLiteral("reserve-expiring"),
        QStringLiteral("charge.reserve"),
        {{QStringLiteral("session_token"), tokenA}, {QStringLiteral("charger_id"), 2}});
    const qint64 expiringId = expiring.data.value(QStringLiteral("id")).toInteger();
    if (!expiring.success || !forceExpired(databasePath, expiringId)) return 21;
    const auto detailExpired = call(clientA, responsesA, QStringLiteral("detail-expired"),
        QStringLiteral("station.detail"), {{QStringLiteral("station_id"), 1}});
    const auto activeExpired = call(clientA, responsesA, QStringLiteral("active-expired"),
        QStringLiteral("charge.active"), {{QStringLiteral("session_token"), tokenA}});
    const QJsonObject expiredReleased = chargerFrom(detailExpired, 2);
    if (!detailExpired.success || !activeExpired.success
        || activeExpired.data.value(QStringLiteral("has_active")).toBool()
        || expiredReleased.value(QStringLiteral("status")).toInt() != 0
        || expiredReleased.value(QStringLiteral("active_order_status")).toInt() != -1) return 22;

    clientA.disconnectFromServer();
    clientB.disconnectFromServer();
    server.shutdown();
    return 0;
}
