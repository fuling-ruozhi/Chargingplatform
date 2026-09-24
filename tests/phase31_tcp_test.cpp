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
    ncs::NetworkServerHost server(directory.filePath(QStringLiteral("p31tcp.db")));
    quint16 port = 0;
    QObject::connect(&server, &ncs::NetworkServerHost::serverStarted,
                     [&](quint16 value) { port = value; });
    server.startServer(QStringLiteral("127.0.0.1"), 0);
    if (!waitUntil([&] { return port > 0; })) return 1;

    ncs::NetworkClient client;
    bool connected = false;
    QList<ncs::JsonResponse> responses;
    QObject::connect(&client, &ncs::NetworkClient::connected,
                     [&] { connected = true; });
    QObject::connect(&client, &ncs::NetworkClient::responseReceived,
                     [&](const auto &response) { responses.append(response); });
    client.connectToServer(QStringLiteral("127.0.0.1"), port);
    if (!waitUntil([&] { return connected; })) return 2;

    auto call = [&](const QString &id, const QString &type, const QJsonObject &data) {
        client.sendRequest(type, data, id);
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

    const auto otp = call(QStringLiteral("otp"), QStringLiteral("user.otp.request"),
                          {{QStringLiteral("phone"), QStringLiteral("13800138003")}});
    const auto wrong = call(QStringLiteral("wrong"), QStringLiteral("user.login"),
        {{QStringLiteral("phone"), QStringLiteral("13800138003")},
         {QStringLiteral("code"), QStringLiteral("111111")}});
    const auto login = call(QStringLiteral("login"), QStringLiteral("user.login"),
        {{QStringLiteral("phone"), QStringLiteral("13800138003")},
         {QStringLiteral("code"), otp.data.value(QStringLiteral("display_code"))}});
    const QJsonObject user = login.data.value(QStringLiteral("user")).toObject();
    const bool ok = otp.success && !wrong.success && login.success
        && login.data.value(QStringLiteral("session_token")).toString().size() == 64
        && user.value(QStringLiteral("nickname")).toString() == QStringLiteral("用户8003");
    client.disconnectFromServer();
    server.shutdown();
    return ok ? 0 : 3;
}
