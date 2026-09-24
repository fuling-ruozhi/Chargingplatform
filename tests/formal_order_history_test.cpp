#include "database/database_manager.h"
#include "model/business_error.h"
#include "network/network_client.h"
#include "network/network_server_host.h"
#include "repository/charge_repository.h"
#include "repository/user_repository.h"
#include "service/charge_service.h"
#include "service/user_service.h"
#include "util/date_time_storage.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>

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

ncs::JsonResponse responseFor(const QList<ncs::JsonResponse> &responses,
                              const QString &id)
{
    for (const auto &response : responses) {
        if (response.requestId == id) return response;
    }
    return {};
}

ncs::JsonResponse call(ncs::NetworkClient &client,
                       QList<ncs::JsonResponse> &responses,
                       const QString &id, const QString &route,
                       const QJsonObject &data)
{
    client.sendRequest(route, data, id);
    waitUntil([&] { return !responseFor(responses, id).requestId.isEmpty(); });
    return responseFor(responses, id);
}

QString login(ncs::NetworkClient &client, QList<ncs::JsonResponse> &responses,
              const QString &suffix, const QString &phone)
{
    const auto otp = call(client, responses, QStringLiteral("otp-") + suffix,
                          QStringLiteral("user.otp.request"),
                          {{QStringLiteral("phone"), phone}});
    const auto result = call(client, responses, QStringLiteral("login-") + suffix,
        QStringLiteral("user.login"),
        {{QStringLiteral("phone"), phone},
         {QStringLiteral("code"), otp.data.value(QStringLiteral("display_code"))}});
    return result.success
        ? result.data.value(QStringLiteral("session_token")).toString() : QString();
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    const QString databasePath = directory.filePath(QStringLiteral("order-history.db"));
    const QString phoneA = QStringLiteral("13800138601");
    const QString phoneB = QStringLiteral("13800138602");
    qint64 userAId = 0;
    qint64 ownNewestId = 0;
    qint64 otherId = 0;
    {
        ncs::DatabaseManager database(databasePath);
        if (!database.initialize()) return 1;
        QSqlQuery chargers(database.connection());
        if (!chargers.exec(QStringLiteral(
                "UPDATE charger SET status=0 WHERE id IN(1,2,3)"))) return 2;
        ncs::UserRepository users(database);
        ncs::UserService userService(database, users);
        const auto otpA = userService.requestOtp(phoneA);
        const auto userA = userService.loginWithOtp(phoneA, otpA.value.displayCode);
        const auto otpB = userService.requestOtp(phoneB);
        const auto userB = userService.loginWithOtp(phoneB, otpB.value.displayCode);
        if (!userA.success || !userB.success
            || !userService.recharge(userA.value.id, 100.0).success
            || !userService.recharge(userB.value.id, 100.0).success) return 3;
        userAId = userA.value.id;
        ncs::ChargeRepository orders(database);
        ncs::ChargeService service(database, orders);
        const auto first = service.reserve(userA.value.id, 1);
        if (!first.success || !service.cancel(userA.value.id, first.value.id).success)
            return 4;
        const auto second = service.reserve(userA.value.id, 2);
        const auto charging = service.startReserved(userA.value.id, second.value.id);
        if (!second.success || !charging.success) return 5;
        QSqlQuery backdate(database.connection());
        backdate.prepare(QStringLiteral(
            "UPDATE charging_order SET start_time=:start WHERE id=:id"));
        backdate.bindValue(QStringLiteral(":start"), ncs::DateTimeStorage::toText(
            ncs::DateTimeStorage::now().addSecs(-30)));
        backdate.bindValue(QStringLiteral(":id"), charging.value.id);
        if (!backdate.exec() || !service.stop(charging.value.id, userA.value.id).success)
            return 6;
        ownNewestId = charging.value.id;
        const auto other = service.reserve(userB.value.id, 3);
        if (!other.success || !service.cancel(userB.value.id, other.value.id).success)
            return 7;
        otherId = other.value.id;
        if (database.connection().tables().contains(QStringLiteral("charging_record")))
            return 8;
    }

    ncs::NetworkServerHost server(databasePath);
    quint16 port = 0;
    QObject::connect(&server, &ncs::NetworkServerHost::serverStarted,
                     [&](quint16 value) { port = value; });
    server.startServer(QStringLiteral("127.0.0.1"), 0);
    if (!waitUntil([&] { return port > 0; })) return 9;

    ncs::NetworkClient clientA;
    QList<ncs::JsonResponse> responsesA;
    bool connected = false;
    QObject::connect(&clientA, &ncs::NetworkClient::connected,
                     [&] { connected = true; });
    QObject::connect(&clientA, &ncs::NetworkClient::responseReceived,
                     [&](const auto &response) { responsesA.append(response); });
    clientA.connectToServer(QStringLiteral("127.0.0.1"), port);
    if (!waitUntil([&] { return connected; })) return 10;
    const QString tokenA = login(clientA, responsesA, QStringLiteral("a"), phoneA);
    if (tokenA.isEmpty()) return 11;

    const auto unauthenticated = call(clientA, responsesA, QStringLiteral("unauth"),
                                      QStringLiteral("order.list"), {});
    if (unauthenticated.success) return 12;
    const auto listed = call(clientA, responsesA, QStringLiteral("list"),
        QStringLiteral("order.list"),
        {{QStringLiteral("session_token"), tokenA},
         {QStringLiteral("user_id"), 99999},
         {QStringLiteral("page"), 1}, {QStringLiteral("page_size"), 20}});
    const QJsonArray items = listed.data.value(QStringLiteral("orders")).toArray();
    if (!listed.success || items.size() != 2
        || items.at(0).toObject().value(QStringLiteral("order_id")).toInteger()
               != ownNewestId) return 13;
    for (const auto &item : items) {
        if (item.toObject().value(QStringLiteral("user_id")).toInteger() != userAId)
            return 14;
    }

    const auto detail = call(clientA, responsesA, QStringLiteral("detail"),
        QStringLiteral("order.detail"),
        {{QStringLiteral("session_token"), tokenA},
         {QStringLiteral("order_id"), ownNewestId}});
    if (!detail.success || detail.data.value(QStringLiteral("status")).toInt() != 2
        || detail.data.value(QStringLiteral("order_no")).toString().isEmpty()
        || detail.data.value(QStringLiteral("end_time")).toString().isEmpty()
        || detail.data.value(QStringLiteral("duration_seconds")).toInteger() <= 0)
        return 15;

    const auto other = call(clientA, responsesA, QStringLiteral("other"),
        QStringLiteral("order.detail"),
        {{QStringLiteral("session_token"), tokenA},
         {QStringLiteral("order_id"), otherId}});
    const auto missing = call(clientA, responsesA, QStringLiteral("missing"),
        QStringLiteral("order.detail"),
        {{QStringLiteral("session_token"), tokenA},
         {QStringLiteral("order_id"), 999999}});
    if (other.success || missing.success || other.code != missing.code
        || other.code != static_cast<int>(ncs::BusinessErrorCode::InvalidOrderOwner))
        return 16;

    const auto badPage = call(clientA, responsesA, QStringLiteral("bad-page"),
        QStringLiteral("order.list"),
        {{QStringLiteral("session_token"), tokenA},
         {QStringLiteral("page"), 1}, {QStringLiteral("page_size"), 101}});
    if (badPage.success) return 17;

    clientA.disconnectFromServer();
    server.shutdown();
    return 0;
}
