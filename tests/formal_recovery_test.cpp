#include "config/charge_config.h"
#include "database/database_manager.h"
#include "network/network_client.h"
#include "network/network_server_host.h"
#include "util/charge_calculator.h"
#include "util/date_time_storage.h"

#include <QCoreApplication>
#include <QElapsedTimer>
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
                              const QString &requestId)
{
    for (const auto &response : responses) {
        if (response.requestId == requestId) return response;
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

ncs::JsonResponse login(ncs::NetworkClient &client,
                        QList<ncs::JsonResponse> &responses,
                        const QString &suffix, const QString &phone)
{
    const auto otp = call(client, responses, QStringLiteral("otp-") + suffix,
                          QStringLiteral("user.otp.request"),
                          {{QStringLiteral("phone"), phone}});
    if (!otp.success) return otp;
    return call(client, responses, QStringLiteral("login-") + suffix,
                QStringLiteral("user.login"),
                {{QStringLiteral("phone"), phone},
                 {QStringLiteral("code"),
                  otp.data.value(QStringLiteral("display_code"))}});
}

bool connectClient(ncs::NetworkClient &client, quint16 port)
{
    bool connected = false;
    QObject::connect(&client, &ncs::NetworkClient::connected,
                     [&] { connected = true; });
    client.connectToServer(QStringLiteral("127.0.0.1"), port);
    return waitUntil([&] { return connected; });
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    const QString databasePath = directory.filePath(QStringLiteral("recovery.db"));
    qint64 chargerId = 0;
    {
        ncs::DatabaseManager setup(databasePath);
        if (!setup.initialize()) return 1;
        QSqlQuery charger(setup.connection());
        if (!charger.exec(QStringLiteral(
                "SELECT id FROM charger WHERE status=0 ORDER BY id LIMIT 1"))
            || !charger.next()) return 2;
        chargerId = charger.value(0).toLongLong();
    }

    ncs::NetworkServerHost server(databasePath);
    quint16 port = 0;
    QObject::connect(&server, &ncs::NetworkServerHost::serverStarted,
                     [&](quint16 value) { port = value; });
    server.startServer(QStringLiteral("127.0.0.1"), 0);
    if (!waitUntil([&] { return port > 0; })) return 3;

    const QString phone = QStringLiteral("13800138401");
    ncs::NetworkClient first;
    QList<ncs::JsonResponse> firstResponses;
    QObject::connect(&first, &ncs::NetworkClient::responseReceived,
                     [&](const auto &response) { firstResponses.append(response); });
    if (!connectClient(first, port)) return 4;
    const auto firstLogin = login(first, firstResponses, QStringLiteral("first"), phone);
    if (!firstLogin.success) return 5;
    const QString firstToken = firstLogin.data.value(QStringLiteral("session_token")).toString();
    const auto recharge = call(first, firstResponses, QStringLiteral("recharge"),
        QStringLiteral("user.recharge"),
        {{QStringLiteral("session_token"), firstToken},
         {QStringLiteral("amount"), 100.0}});
    const auto reserved = call(first, firstResponses, QStringLiteral("reserve"),
        QStringLiteral("charge.reserve"),
        {{QStringLiteral("session_token"), firstToken},
         {QStringLiteral("charger_id"), chargerId}});
    if (!recharge.success || !reserved.success) return 6;
    const qint64 orderId = reserved.data.value(QStringLiteral("order_id")).toInteger();
    const QString expireAt = reserved.data.value(QStringLiteral("expire_at")).toString();
    first.disconnectFromServer();

    ncs::NetworkClient second;
    QList<ncs::JsonResponse> secondResponses;
    QObject::connect(&second, &ncs::NetworkClient::responseReceived,
                     [&](const auto &response) { secondResponses.append(response); });
    if (!connectClient(second, port)) return 7;
    const auto secondLogin = login(second, secondResponses, QStringLiteral("second"), phone);
    if (!secondLogin.success) return 8;
    const QString secondToken = secondLogin.data.value(QStringLiteral("session_token")).toString();
    const auto recoveredReservation = call(second, secondResponses,
        QStringLiteral("active-reserved"), QStringLiteral("charge.active"),
        {{QStringLiteral("session_token"), secondToken}});
    if (!recoveredReservation.success
        || !recoveredReservation.data.value(QStringLiteral("has_active")).toBool()
        || recoveredReservation.data.value(QStringLiteral("order_id")).toInteger() != orderId
        || recoveredReservation.data.value(QStringLiteral("status")).toInt() != 0
        || recoveredReservation.data.value(QStringLiteral("expire_at")).toString() != expireAt)
        return 9;

    const auto started = call(second, secondResponses, QStringLiteral("start"),
        QStringLiteral("charge.start"),
        {{QStringLiteral("session_token"), secondToken},
         {QStringLiteral("order_id"), orderId}});
    if (!started.success || started.data.value(QStringLiteral("status")).toInt() != 1)
        return 10;
    const QString backdated = ncs::DateTimeStorage::toText(
        ncs::DateTimeStorage::now().addSecs(-30));
    {
        ncs::DatabaseManager edit(databasePath);
        if (!edit.open()) return 11;
        QSqlQuery update(edit.connection());
        update.prepare(QStringLiteral(
            "UPDATE charging_order SET start_time=:start WHERE id=:id"));
        update.bindValue(QStringLiteral(":start"), backdated);
        update.bindValue(QStringLiteral(":id"), orderId);
        if (!update.exec()) return 12;
    }
    second.disconnectFromServer();

    ncs::NetworkClient third;
    QList<ncs::JsonResponse> thirdResponses;
    QObject::connect(&third, &ncs::NetworkClient::responseReceived,
                     [&](const auto &response) { thirdResponses.append(response); });
    if (!connectClient(third, port)) return 13;
    const auto thirdLogin = login(third, thirdResponses, QStringLiteral("third"), phone);
    if (!thirdLogin.success) return 14;
    const QString thirdToken = thirdLogin.data.value(QStringLiteral("session_token")).toString();
    const auto recoveredCharging = call(third, thirdResponses,
        QStringLiteral("active-charging"), QStringLiteral("charge.active"),
        {{QStringLiteral("session_token"), thirdToken}});
    if (!recoveredCharging.success
        || recoveredCharging.data.value(QStringLiteral("order_id")).toInteger() != orderId
        || recoveredCharging.data.value(QStringLiteral("status")).toInt() != 1
        || recoveredCharging.data.value(QStringLiteral("start_time")).toString() != backdated)
        return 15;

    const auto metrics = ncs::ChargeCalculator::calculate(
        ncs::DateTimeStorage::fromText(backdated), ncs::DateTimeStorage::now(),
        recoveredCharging.data.value(QStringLiteral("power_kw")).toDouble(),
        recoveredCharging.data.value(QStringLiteral("price_per_kwh")).toDouble(),
        recoveredCharging.data.value(QStringLiteral("time_scale")).toInt(),
        recoveredCharging.data.value(QStringLiteral("initial_soc")).toDouble(),
        ncs::ChargeConfig::batteryCapacityKwh());
    if (metrics.elapsedSimSeconds < 1800 || metrics.energyKwh <= 0.0
        || metrics.amount <= 0.0) return 16;

    third.disconnectFromServer();
    server.shutdown();
    return 0;
}
