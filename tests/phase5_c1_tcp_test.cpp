#include "database/database_manager.h"
#include "network/network_client.h"
#include "network/network_server_host.h"

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
    for (const ncs::JsonResponse &response : responses) {
        if (response.requestId == requestId) return response;
    }
    return {};
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    const QString databasePath = directory.filePath(QStringLiteral("c1-tcp.db"));
    {
        ncs::DatabaseManager setup(databasePath);
        if (!setup.initialize()) return 1;
        QSqlQuery query(setup.connection());
        if (!query.exec(QStringLiteral("UPDATE charger SET status=0 WHERE id IN (1,2)"))) return 2;
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
                    const QString &id, const QString &type, const QJsonObject &data) {
        client.sendRequest(type, data, id);
        waitUntil([&] {
            for (const ncs::JsonResponse &response : responses) {
                if (response.requestId == id) return true;
            }
            return false;
        });
        return responseFor(responses, id);
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
                              QStringLiteral("13800138301"));
    const auto loginB = login(clientB, responsesB, QStringLiteral("b"),
                              QStringLiteral("13800138302"));
    if (!loginA.success || !loginB.success) return 5;
    const QString tokenA = loginA.data.value(QStringLiteral("session_token")).toString();
    const QString tokenB = loginB.data.value(QStringLiteral("session_token")).toString();
    const auto rechargeA = call(clientA, responsesA, QStringLiteral("recharge-a"),
        QStringLiteral("user.recharge"),
        {{QStringLiteral("session_token"), tokenA}, {QStringLiteral("amount"), 10.0}});
    const auto rechargeB = call(clientB, responsesB, QStringLiteral("recharge-b"),
        QStringLiteral("user.recharge"),
        {{QStringLiteral("session_token"), tokenB}, {QStringLiteral("amount"), 10.0}});
    if (!rechargeA.success || !rechargeB.success) return 17;

    clientA.sendRequest(QStringLiteral("charge.reserve"),
        {{QStringLiteral("session_token"), tokenA}, {QStringLiteral("charger_id"), 1}},
        QStringLiteral("reserve-a"));
    clientB.sendRequest(QStringLiteral("charge.reserve"),
        {{QStringLiteral("session_token"), tokenB}, {QStringLiteral("charger_id"), 1}},
        QStringLiteral("reserve-b"));
    if (!waitUntil([&] {
            return responseFor(responsesA, QStringLiteral("reserve-a")).requestId
                       == QStringLiteral("reserve-a")
                && responseFor(responsesB, QStringLiteral("reserve-b")).requestId
                       == QStringLiteral("reserve-b");
        })) return 6;

    const auto reserveA = responseFor(responsesA, QStringLiteral("reserve-a"));
    const auto reserveB = responseFor(responsesB, QStringLiteral("reserve-b"));
    if (reserveA.success == reserveB.success) return 7;

    ncs::NetworkClient *winnerClient = reserveA.success ? &clientA : &clientB;
    QList<ncs::JsonResponse> *winnerResponses = reserveA.success ? &responsesA : &responsesB;
    const QString winnerToken = reserveA.success ? tokenA : tokenB;
    const auto winnerReservation = reserveA.success ? reserveA : reserveB;
    const qint64 recordId = winnerReservation.data.value(QStringLiteral("order_id")).toInteger();
    if (recordId <= 0 || winnerReservation.data.value(QStringLiteral("status")).toInt() != 0)
        return 8;

    const auto activeReserved = call(*winnerClient, *winnerResponses,
        QStringLiteral("active-reserved"), QStringLiteral("charge.active"),
        {{QStringLiteral("session_token"), winnerToken}});
    if (!activeReserved.success
        || !activeReserved.data.value(QStringLiteral("has_active")).toBool()
        || activeReserved.data.value(QStringLiteral("status")).toInt() != 0) return 9;

    const auto started = call(*winnerClient, *winnerResponses,
        QStringLiteral("start"), QStringLiteral("charge.start"),
        {{QStringLiteral("session_token"), winnerToken},
         {QStringLiteral("order_id"), recordId}});
    if (!started.success || started.data.value(QStringLiteral("order_id")).toInteger() != recordId
        || started.data.value(QStringLiteral("status")).toInt() != 1) return 10;

    const auto activeCharging = call(*winnerClient, *winnerResponses,
        QStringLiteral("active-charging"), QStringLiteral("charge.active"),
        {{QStringLiteral("session_token"), winnerToken}});
    if (!activeCharging.success
        || activeCharging.data.value(QStringLiteral("status")).toInt() != 1) return 11;

    const auto cancelCharging = call(*winnerClient, *winnerResponses,
        QStringLiteral("cancel-charging"), QStringLiteral("charge.cancel"),
        {{QStringLiteral("session_token"), winnerToken},
         {QStringLiteral("order_id"), recordId}});
    if (cancelCharging.success) return 12;

    const auto stopped = call(*winnerClient, *winnerResponses,
        QStringLiteral("stop"), QStringLiteral("charge.settle"),
        {{QStringLiteral("session_token"), winnerToken},
         {QStringLiteral("order_id"), recordId}});
    if (!stopped.success || stopped.data.value(QStringLiteral("status")).toInt() != 2)
        return 13;

    const auto reservedAgain = call(*winnerClient, *winnerResponses,
        QStringLiteral("reserve-again"), QStringLiteral("charge.reserve"),
        {{QStringLiteral("session_token"), winnerToken}, {QStringLiteral("charger_id"), 2}});
    if (!reservedAgain.success) return 14;
    const auto cancelled = call(*winnerClient, *winnerResponses,
        QStringLiteral("cancel"), QStringLiteral("charge.cancel"),
        {{QStringLiteral("session_token"), winnerToken},
         {QStringLiteral("order_id"),
          reservedAgain.data.value(QStringLiteral("order_id")).toInteger()}});
    if (!cancelled.success || cancelled.data.value(QStringLiteral("status")).toInt() != 3)
        return 15;

    const auto noActive = call(*winnerClient, *winnerResponses,
        QStringLiteral("active-none"), QStringLiteral("charge.active"),
        {{QStringLiteral("session_token"), winnerToken}});
    if (!noActive.success || noActive.data.value(QStringLiteral("has_active")).toBool())
        return 16;

    clientA.disconnectFromServer();
    clientB.disconnectFromServer();
    server.shutdown();
    return 0;
}
