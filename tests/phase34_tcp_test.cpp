#include "model/business_error.h"
#include "network/network_client.h"
#include "network/network_server_host.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QThread>

#include <functional>

namespace {
bool waitUntil(const std::function<bool()> &condition)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < 4000) {
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
    ncs::NetworkServerHost server(directory.filePath(QStringLiteral("phase34tcp.db")));
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
    auto call = [&](const QString &id, const QString &type, const QJsonObject &data) {
        client.sendRequest(type, data, id);
        waitUntil([&] { for (const auto &r : responses) if (r.requestId == id) return true; return false; });
        for (const auto &r : responses) if (r.requestId == id) return r;
        return ncs::JsonResponse();
    };
    const auto otp = call(QStringLiteral("otp"), QStringLiteral("user.otp.request"),
        {{QStringLiteral("phone"), QStringLiteral("13800138201")}});
    const auto login = call(QStringLiteral("login"), QStringLiteral("user.login"),
        {{QStringLiteral("phone"), QStringLiteral("13800138201")},
         {QStringLiteral("code"), otp.data.value(QStringLiteral("display_code"))}});
    const QString token = login.data.value(QStringLiteral("session_token")).toString();
    const auto recharge = call(QStringLiteral("recharge"), QStringLiteral("user.recharge"),
        {{QStringLiteral("session_token"), token}, {QStringLiteral("amount"), 10.0}});
    const QJsonObject ids{{QStringLiteral("session_token"), token},
                          {QStringLiteral("charger_id"), 1}};
    const auto started = call(QStringLiteral("start"), QStringLiteral("charge.start"), ids);
    const auto duplicate = call(QStringLiteral("duplicate"), QStringLiteral("charge.start"), ids);
    const auto stopped = call(QStringLiteral("stop"), QStringLiteral("charge.settle"),
        {{QStringLiteral("session_token"), token},
         {QStringLiteral("order_id"), started.data.value(QStringLiteral("order_id")).toInteger()}});
    const bool ok = otp.success && login.success && recharge.success && started.success
        && !duplicate.success
        && duplicate.code == static_cast<int>(ncs::BusinessErrorCode::ChargerUnavailable)
        && stopped.success
        && !stopped.data.value(QStringLiteral("end_time")).toString().isEmpty()
        && stopped.data.value(QStringLiteral("energy")).toDouble() >= 0
        && stopped.data.value(QStringLiteral("amount")).toDouble() >= 0
        && stopped.data.value(QStringLiteral("duration_seconds")).toInteger() >= 0;
    client.disconnectFromServer();
    server.shutdown();
    return ok ? 0 : 3;
}
