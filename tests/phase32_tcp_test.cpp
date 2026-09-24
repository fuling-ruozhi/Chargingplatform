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
    ncs::NetworkServerHost server(directory.filePath(QStringLiteral("phase32tcp.db")));
    quint16 port = 0;
    QObject::connect(&server, &ncs::NetworkServerHost::serverStarted,
                     [&](quint16 value) { port = value; });
    server.startServer(QStringLiteral("127.0.0.1"), 0);
    if (!waitUntil([&] { return port > 0; })) return 1;
    ncs::NetworkClient client;
    bool connected = false;
    ncs::JsonResponse response;
    bool received = false;
    QObject::connect(&client, &ncs::NetworkClient::connected, [&] { connected = true; });
    QObject::connect(&client, &ncs::NetworkClient::responseReceived,
                     [&](const ncs::JsonResponse &value) { response = value; received = true; });
    client.connectToServer(QStringLiteral("127.0.0.1"), port);
    if (!waitUntil([&] { return connected; })) return 2;
    client.sendRequest(QStringLiteral("station.list"), QJsonObject(), QStringLiteral("list"));
    if (!waitUntil([&] { return received; })) return 3;
    const QJsonArray stations = response.data.value(QStringLiteral("stations")).toArray();
    const bool ok = response.success && stations.size() == 5
        && stations.first().toObject().value(QStringLiteral("name")).toString()
            == QStringLiteral("BIT充电站");
    client.disconnectFromServer();
    server.shutdown();
    return ok ? 0 : 4;
}
