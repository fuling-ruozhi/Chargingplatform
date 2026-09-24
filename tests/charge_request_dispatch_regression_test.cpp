#include "network/frame_codec.h"
#include "network/json_protocol.h"
#include "service/user_client_facade.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>

#include <functional>
#include <iostream>

namespace {

bool waitUntil(const std::function<bool()> &condition, int timeoutMs = 1000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1);
    }
    return condition();
}

}

namespace ncs {

class UserClientFacadeTestProbe
{
public:
    static void authenticate(UserClientFacade &facade)
    {
        facade.sessionToken_ = QStringLiteral("test-session");
    }

    static int pendingCount(const UserClientFacade &facade)
    {
        return facade.requests_->pendingCount();
    }

    static void expire(UserClientFacade &facade, const QString &requestId)
    {
        facade.expireRequest(requestId);
    }

    static void addPending(UserClientFacade &facade, const QString &requestId,
                           const QString &route)
    {
        facade.requests_->track(
            requestId, route,
            RequestLifecycleManager::DuplicatePolicy::Allow, 60000);
    }

    static void deliver(UserClientFacade &facade, const JsonResponse &response)
    {
        facade.handle(response);
    }
};

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) return 1;

    ncs::FrameCodec serverCodec;
    QTcpSocket *peer = nullptr;
    QObject::connect(&server, &QTcpServer::newConnection, [&] {
        std::cerr << "server-connected\n";
        peer = server.nextPendingConnection();
        QObject::connect(peer, &QTcpSocket::readyRead, [&] {
            QList<QByteArray> frames;
            QString frameError;
            if (!serverCodec.append(peer->readAll(), &frames, &frameError)) return;
            for (const QByteArray &frame : frames) {
                ncs::JsonRequest request;
                ncs::ProtocolError protocolError;
                if (!ncs::JsonProtocol::decodeRequest(frame, &request, &protocolError)) continue;
                QJsonObject data{{QStringLiteral("order_id"), 41},
                                 {QStringLiteral("charger_id"), 7},
                                 {QStringLiteral("charger_code"),
                                  QStringLiteral("NCS-01-01")}};
                if (request.type == QStringLiteral("charge.reserve")) {
                    data.insert(QStringLiteral("status"), 0);
                } else if (request.type == QStringLiteral("charge.start")) {
                    data.insert(QStringLiteral("status"), 1);
                } else if (request.type == QStringLiteral("charge.cancel")) {
                    data.insert(QStringLiteral("status"), 3);
                }
                const auto response = ncs::JsonProtocol::success(request.requestId, data);
                peer->write(ncs::FrameCodec::encode(
                    ncs::JsonProtocol::encodeResponse(response)));
                std::cerr << "server-response=" << request.requestId.toStdString() << '\n';
            }
        });
    });

    ncs::UserClientFacade client;
    bool connected = false;
    bool secondReceivedInsideNestedLoop = false;
    bool firstIdMatched = false;
    bool secondIdMatched = false;
    bool startScenario = false;
    bool startReceived = false;
    bool startReceivedInsideNestedLoop = false;
    QEventLoop *nestedLoop = nullptr;
    QObject::connect(&client, &ncs::UserClientFacade::connected,
                     [&] { connected = true; });
    QObject::connect(&client, &ncs::UserClientFacade::reservationCreated,
                     [&](const ncs::ChargingRecord &record) {
        if (record.id == 41) {
            std::cerr << "client-first\n";
            firstIdMatched = true;
            if (startScenario) client.startCharge(1, record.chargerId, record.id);
            else client.cancelReservation(1, record.id);
            QEventLoop loop;
            nestedLoop = &loop;
            QTimer::singleShot(300, &loop, &QEventLoop::quit);
            loop.exec();
            std::cerr << "nested-exit response="
                      << (startScenario ? startReceived : secondIdMatched) << '\n';
            nestedLoop = nullptr;
            if (startScenario) startReceivedInsideNestedLoop = startReceived;
            else secondReceivedInsideNestedLoop = secondIdMatched;
        }
    });
    QObject::connect(&client, &ncs::UserClientFacade::reservationCancelled,
                     [&](const ncs::ChargingRecord &record) {
        if (record.id != 41) return;
        std::cerr << "client-second\n";
        secondIdMatched = true;
        if (nestedLoop) nestedLoop->quit();
    });
    QObject::connect(&client, &ncs::UserClientFacade::chargeStarted,
                     [&](const ncs::ChargingRecord &record) {
        if (record.id != 41) return;
        std::cerr << "client-started\n";
        startReceived = true;
        if (nestedLoop) nestedLoop->quit();
    });

    client.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort());
    if (!waitUntil([&] { return connected && peer; })) return 2;
    std::cerr << "client-connected\n";
    ncs::UserClientFacadeTestProbe::authenticate(client);
    client.reserveCharge(1, 7);
    if (!waitUntil([&] { return firstIdMatched; })) return 4;
    if (!secondReceivedInsideNestedLoop) return 5;
    if (!secondIdMatched) return 6;
    startScenario = true;
    firstIdMatched = false;
    client.reserveCharge(1, 7);
    if (!waitUntil([&] { return firstIdMatched; })
        || !startReceivedInsideNestedLoop || !startReceived) return 7;
    std::cerr << "complete\n";
    client.disconnectFromServer();
    if (peer) peer->disconnectFromHost();
    QCoreApplication::processEvents();

    ncs::UserClientFacade facade;
    QString failedRoute;
    int activeSignals = 0;
    QObject::connect(&facade, &ncs::UserClientFacade::requestFailed,
                     [&](const QString &route, int, const QString &) {
        failedRoute = route;
    });
    QObject::connect(&facade, &ncs::UserClientFacade::activeChargeReceived,
                     [&](bool, const ncs::ChargingRecord &) { ++activeSignals; });
    const QString fakeId = QStringLiteral("expired-active-request");
    ncs::UserClientFacadeTestProbe::addPending(
        facade, fakeId, QStringLiteral("charge.active"));
    if (ncs::UserClientFacadeTestProbe::pendingCount(facade) != 1) return 8;
    const QString expiredId = fakeId;
    ncs::UserClientFacadeTestProbe::expire(facade, expiredId);
    if (ncs::UserClientFacadeTestProbe::pendingCount(facade) != 0
        || failedRoute != QStringLiteral("charge.active")) return 9;
    ncs::UserClientFacadeTestProbe::deliver(
        facade, ncs::JsonProtocol::success(
                    expiredId, {{QStringLiteral("has_active"), false}}));
    if (activeSignals != 0) return 10;
    return 0;
}
