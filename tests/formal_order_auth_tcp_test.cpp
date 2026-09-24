#include "model/business_error.h"
#include "network/network_client.h"
#include "network/network_server_host.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
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

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::NetworkServerHost server(directory.filePath(QStringLiteral("order-auth.db")));
    quint16 port = 0;
    QObject::connect(&server, &ncs::NetworkServerHost::serverStarted,
                     [&](quint16 value) { port = value; });
    server.startServer(QStringLiteral("127.0.0.1"), 0);
    if (!waitUntil([&] { return port > 0; })) return 1;

    ncs::NetworkClient client;
    bool connected = false;
    QList<ncs::JsonResponse> responses;
    QObject::connect(&client, &ncs::NetworkClient::connected, [&] { connected = true; });
    QObject::connect(&client, &ncs::NetworkClient::responseReceived,
                     [&](const ncs::JsonResponse &value) { responses.append(value); });
    client.connectToServer(QStringLiteral("127.0.0.1"), port);
    if (!waitUntil([&] { return connected; })) return 2;

    auto call = [&](const QString &id, const QString &route, const QJsonObject &data) {
        client.sendRequest(route, data, id);
        waitUntil([&] {
            for (const auto &response : responses) {
                if (response.requestId == id) return true;
            }
            return false;
        });
        for (const auto &response : responses) {
            if (response.requestId == id) return response;
        }
        return ncs::JsonResponse();
    };
    auto login = [&](const QString &suffix, const QString &phone) {
        const auto otp = call(QStringLiteral("otp-") + suffix,
                              QStringLiteral("user.otp.request"),
                              {{QStringLiteral("phone"), phone}});
        return call(QStringLiteral("login-") + suffix, QStringLiteral("user.login"),
                    {{QStringLiteral("phone"), phone},
                     {QStringLiteral("code"),
                      otp.data.value(QStringLiteral("display_code"))}});
    };

    const auto loginA = login(QStringLiteral("a"), QStringLiteral("13800138101"));
    const auto loginB = login(QStringLiteral("b"), QStringLiteral("13800138102"));
    if (!loginA.success || !loginB.success) return 3;
    const QString tokenA = loginA.data.value(QStringLiteral("session_token")).toString();
    const QString tokenB = loginB.data.value(QStringLiteral("session_token")).toString();
    const qint64 userA = loginA.data.value(QStringLiteral("user")).toObject()
                               .value(QStringLiteral("id")).toInteger();
    const qint64 userB = loginB.data.value(QStringLiteral("user")).toObject()
                               .value(QStringLiteral("id")).toInteger();
    const auto recharge = call(QStringLiteral("recharge"), QStringLiteral("user.recharge"),
        {{QStringLiteral("session_token"), tokenA}, {QStringLiteral("amount"), 10.0}});
    if (!recharge.success) return 12;

    const auto detail = call(QStringLiteral("detail"), QStringLiteral("station.detail"),
                             {{QStringLiteral("station_id"), 1}});
    qint64 chargerId = 0;
    for (const QJsonValue value : detail.data.value(QStringLiteral("chargers")).toArray()) {
        const QJsonObject charger = value.toObject();
        if (charger.value(QStringLiteral("status")).toInt() == 0) {
            chargerId = charger.value(QStringLiteral("id")).toInteger();
            break;
        }
    }
    if (!detail.success || chargerId <= 0) return 4;

    const auto missing = call(QStringLiteral("missing"), QStringLiteral("charge.reserve"),
                              {{QStringLiteral("charger_id"), chargerId}});
    if (missing.success || missing.code
        != static_cast<int>(ncs::BusinessErrorCode::AuthRequired)) return 5;
    const auto reserved = call(QStringLiteral("reserve"), QStringLiteral("charge.reserve"),
        {{QStringLiteral("session_token"), tokenA},
         {QStringLiteral("user_id"), userB},
         {QStringLiteral("charger_id"), chargerId}});
    if (!reserved.success || reserved.data.value(QStringLiteral("user_id")).toInteger() != userA)
        return 6;
    const qint64 orderId = reserved.data.value(QStringLiteral("order_id")).toInteger();

    const auto foreignStart = call(QStringLiteral("foreign-start"),
        QStringLiteral("charge.start"),
        {{QStringLiteral("session_token"), tokenB}, {QStringLiteral("order_id"), orderId}});
    if (foreignStart.success || foreignStart.code
        != static_cast<int>(ncs::BusinessErrorCode::InvalidOrderOwner)) return 7;
    const auto started = call(QStringLiteral("start"), QStringLiteral("charge.start"),
        {{QStringLiteral("session_token"), tokenA}, {QStringLiteral("order_id"), orderId}});
    if (!started.success || started.data.value(QStringLiteral("status")).toInt() != 1) return 8;

    const auto foreignSettle = call(QStringLiteral("foreign-settle"),
        QStringLiteral("charge.settle"),
        {{QStringLiteral("session_token"), tokenB}, {QStringLiteral("order_id"), orderId}});
    if (foreignSettle.success || foreignSettle.code
        != static_cast<int>(ncs::BusinessErrorCode::InvalidOrderOwner)) return 9;
    const auto settled = call(QStringLiteral("settle"), QStringLiteral("charge.settle"),
        {{QStringLiteral("session_token"), tokenA}, {QStringLiteral("order_id"), orderId}});
    if (!settled.success || settled.data.value(QStringLiteral("status")).toInt() != 2
        || settled.data.value(QStringLiteral("amount")).toDouble() < 0) return 10;

    const auto activeWithoutToken = call(QStringLiteral("active-missing"),
                                         QStringLiteral("charge.active"), {});
    if (activeWithoutToken.success || activeWithoutToken.code
        != static_cast<int>(ncs::BusinessErrorCode::AuthRequired)) return 11;
    client.disconnectFromServer();
    server.shutdown();
    return 0;
}
