#include "network/network_client.h"
#include "network/network_server_host.h"
#include "model/business_error.h"

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
    ncs::NetworkServerHost server(directory.filePath(QStringLiteral("phase33tcp.db")));
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
    auto call = [&](const QString &id, qint64 stationId) {
        client.sendRequest(QStringLiteral("station.detail"),
                           {{QStringLiteral("station_id"), stationId}}, id);
        waitUntil([&] { for (const auto &r : responses) if (r.requestId == id) return true; return false; });
        for (const auto &r : responses) if (r.requestId == id) return r;
        return ncs::JsonResponse();
    };
    const auto detail = call(QStringLiteral("detail"), 1);
    const auto missing = call(QStringLiteral("missing"), 999999);
    const bool ok = detail.success
        && detail.data.value(QStringLiteral("chargers")).toArray().size() == 8
        && !missing.success
        && missing.code == static_cast<int>(ncs::BusinessErrorCode::StationNotFound);
    client.disconnectFromServer();
    server.shutdown();
    return ok ? 0 : 3;
}
