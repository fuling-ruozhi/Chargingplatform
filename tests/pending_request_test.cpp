#include "network/frame_codec.h"
#include "network/json_protocol.h"
#include "service/user_client_facade.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QJsonArray>
#include <QMap>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include <functional>

namespace {

bool waitUntil(const std::function<bool()> &condition, int timeoutMs = 3000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QThread::msleep(1);
    }
    return condition();
}

}

namespace ncs {

class AsyncLifecycleTestProbe
{
public:
    static void authenticate(UserClientFacade &facade)
    {
        facade.sessionToken_ = QStringLiteral("test-session-token");
    }
    static QString token(const UserClientFacade &facade)
    {
        return facade.sessionToken_;
    }
    static int pending(const UserClientFacade &facade)
    {
        return facade.requests_->pendingCount();
    }
    static quint64 generation(const UserClientFacade &facade)
    {
        return facade.requests_->sessionGeneration();
    }
    static void expire(UserClientFacade &facade, const QString &requestId)
    {
        facade.expireRequest(requestId);
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
    QTcpSocket *peer = nullptr;
    ncs::FrameCodec codec;
    QMap<QString, QStringList> idsByRoute;
    QObject::connect(&server, &QTcpServer::newConnection, [&] {
        peer = server.nextPendingConnection();
        QObject::connect(peer, &QTcpSocket::readyRead, [&] {
            QList<QByteArray> frames;
            QString frameError;
            if (!codec.append(peer->readAll(), &frames, &frameError)) return;
            for (const QByteArray &frame : frames) {
                ncs::JsonRequest request;
                ncs::ProtocolError error;
                if (ncs::JsonProtocol::decodeRequest(frame, &request, &error)) {
                    idsByRoute[request.type].append(request.requestId);
                }
            }
        });
    });

    ncs::UserClientFacade facade;
    bool connected = false;
    int stationSignals = 0;
    int rechargeSignals = 0;
    int duplicateSignals = 0;
    QStringList failedRoutes;
    QObject::connect(&facade, &ncs::UserClientFacade::connected,
                     [&] { connected = true; });
    QObject::connect(&facade, &ncs::UserClientFacade::stationsReceived,
                     [&](const QVector<ncs::Station> &) { ++stationSignals; });
    QObject::connect(&facade, &ncs::UserClientFacade::rechargeSucceeded,
                     [&](const ncs::RechargeResult &) { ++rechargeSignals; });
    QObject::connect(&facade, &ncs::UserClientFacade::requestSuppressed,
                     [&](const QString &) { ++duplicateSignals; });
    QObject::connect(&facade, &ncs::UserClientFacade::requestFailed,
                     [&](const QString &route, int, const QString &) {
                         failedRoutes.append(route);
                     });
    facade.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort());
    if (!waitUntil([&] { return connected && peer; })) return 2;
    ncs::AsyncLifecycleTestProbe::authenticate(facade);

    facade.requestStations();
    facade.requestStations();
    if (!waitUntil([&] {
            return idsByRoute[QStringLiteral("station.list")].size() == 2;
        })) return 3;
    const QString oldQuery = idsByRoute[QStringLiteral("station.list")][0];
    const QString newQuery = idsByRoute[QStringLiteral("station.list")][1];
    if (oldQuery == newQuery || ncs::AsyncLifecycleTestProbe::pending(facade) != 1)
        return 4;
    const QJsonObject stationPayload{{QStringLiteral("stations"), QJsonArray()}};
    ncs::AsyncLifecycleTestProbe::deliver(
        facade, ncs::JsonProtocol::success(oldQuery, stationPayload));
    if (stationSignals != 0) return 5;
    ncs::AsyncLifecycleTestProbe::deliver(
        facade, ncs::JsonProtocol::success(newQuery, stationPayload));
    if (stationSignals != 1 || ncs::AsyncLifecycleTestProbe::pending(facade) != 0)
        return 6;

    facade.recharge(10.0);
    facade.recharge(10.0);
    if (!waitUntil([&] {
            return idsByRoute[QStringLiteral("user.recharge")].size() == 1;
        })) return 7;
    if (duplicateSignals != 1 || ncs::AsyncLifecycleTestProbe::pending(facade) != 1)
        return 8;
    const QString expired = idsByRoute[QStringLiteral("user.recharge")].first();
    ncs::AsyncLifecycleTestProbe::expire(facade, expired);
    if (!failedRoutes.contains(QStringLiteral("user.recharge"))
        || ncs::AsyncLifecycleTestProbe::pending(facade) != 0)
        return 9;
    facade.recharge(10.0);
    if (!waitUntil([&] {
            return idsByRoute[QStringLiteral("user.recharge")].size() == 2;
        })) return 10;
    const QString retry = idsByRoute[QStringLiteral("user.recharge")].last();
    if (retry == expired) return 11;
    const QJsonObject rechargePayload{
        {QStringLiteral("log_id"), 1}, {QStringLiteral("amount"), 10.0},
        {QStringLiteral("balance_before"), 0.0},
        {QStringLiteral("balance_after"), 10.0}};
    ncs::AsyncLifecycleTestProbe::deliver(
        facade, ncs::JsonProtocol::success(expired, rechargePayload));
    if (rechargeSignals != 0) return 12;
    ncs::AsyncLifecycleTestProbe::deliver(
        facade, ncs::JsonProtocol::success(retry, rechargePayload));
    if (rechargeSignals != 1 || ncs::AsyncLifecycleTestProbe::pending(facade) != 0)
        return 13;

    auto verifyTransactionalDuplicate =
        [&](const QString &route, const std::function<void()> &send) {
            const int priorRequests = idsByRoute[route].size();
            const int priorSuppressed = duplicateSignals;
            send();
            send();
            if (!waitUntil([&] {
                    return idsByRoute[route].size() == priorRequests + 1;
                })) return false;
            if (duplicateSignals != priorSuppressed + 1
                || ncs::AsyncLifecycleTestProbe::pending(facade) != 1)
                return false;
            ncs::AsyncLifecycleTestProbe::expire(
                facade, idsByRoute[route].last());
            return ncs::AsyncLifecycleTestProbe::pending(facade) == 0;
        };
    if (!verifyTransactionalDuplicate(QStringLiteral("charge.reserve"),
                                      [&] { facade.reserveCharge(1, 7); }))
        return 18;
    if (!verifyTransactionalDuplicate(QStringLiteral("charge.start"),
                                      [&] { facade.startCharge(1, 7, 41); }))
        return 19;
    if (!verifyTransactionalDuplicate(QStringLiteral("charge.cancel"),
                                      [&] { facade.cancelReservation(1, 41); }))
        return 20;
    if (!verifyTransactionalDuplicate(QStringLiteral("charge.settle"),
                                      [&] { facade.stopCharge(41); }))
        return 21;

    auto *receiver = new QObject;
    int destroyedReceiverSignals = 0;
    QObject::connect(&facade, &ncs::UserClientFacade::profileReceived, receiver,
                     [&](const ncs::User &) { ++destroyedReceiverSignals; });
    delete receiver;
    const quint64 oldGeneration = ncs::AsyncLifecycleTestProbe::generation(facade);
    facade.requestProfile();
    if (!waitUntil([&] {
            return idsByRoute[QStringLiteral("user.profile.get")].size() == 1;
        })) return 14;
    const QString disconnectedRequest =
        idsByRoute[QStringLiteral("user.profile.get")].first();
    facade.disconnectFromServer();
    if (!waitUntil([&] { return !facade.isConnected(); })) return 15;
    if (ncs::AsyncLifecycleTestProbe::pending(facade) != 0
        || !ncs::AsyncLifecycleTestProbe::token(facade).isEmpty()
        || ncs::AsyncLifecycleTestProbe::generation(facade) != oldGeneration + 1
        || !failedRoutes.contains(QStringLiteral("user.profile.get")))
        return 16;
    ncs::AsyncLifecycleTestProbe::deliver(
        facade, ncs::JsonProtocol::success(disconnectedRequest, QJsonObject()));
    if (destroyedReceiverSignals != 0) return 17;
    return 0;
}
