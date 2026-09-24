#include "network_client.h"

#include "util/logger.h"

#include <QAbstractSocket>
#include <QTcpSocket>
#include <QUuid>

namespace ncs {

namespace {

bool isChargeLifecycleRoute(const QString &route)
{
    return route == QStringLiteral("charge.reserve")
        || route == QStringLiteral("charge.start")
        || route == QStringLiteral("charge.cancel")
        || route == QStringLiteral("charge.active")
        || route == QStringLiteral("charge.settle");
}

}

NetworkClient::NetworkClient(QObject *parent)
    : QObject(parent), socket_(new QTcpSocket(this))
{
    qRegisterMetaType<ncs::JsonResponse>();
    connect(socket_, &QTcpSocket::connected, this, &NetworkClient::connected);
    connect(socket_, &QTcpSocket::disconnected, this, [this]() {
        codec_.reset();
        tracedChargeRequests_.clear();
        Logger::warning(QStringLiteral("network-client"),
                        QStringLiteral("NETWORK DISCONNECT"));
        emit disconnected();
    });
    connect(socket_, &QTcpSocket::readyRead, this, &NetworkClient::readResponses);
    connect(socket_, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
                emit networkError(socket_->errorString());
            });
}

NetworkClient::~NetworkClient()
{
    disconnect(socket_, nullptr, this, nullptr);
    socket_->abort();
}

bool NetworkClient::isConnected() const
{
    return socket_->state() == QAbstractSocket::ConnectedState;
}

void NetworkClient::connectToServer(const QString &host, quint16 port)
{
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->abort();
    }
    codec_.reset();
    tracedChargeRequests_.clear();
    socket_->connectToHost(host, port);
}

void NetworkClient::disconnectFromServer()
{
    if (socket_->state() == QAbstractSocket::UnconnectedState) {
        return;
    }
    socket_->disconnectFromHost();
}

QString NetworkClient::sendRequest(const QString &type,
                                   const QJsonObject &data,
                                   const QString &requestId)
{
    if (!isConnected()) {
        emit networkError(QStringLiteral("cannot send request while disconnected"));
        return QString();
    }

    const QString effectiveId = requestId.isEmpty()
                                    ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                                    : requestId;
    const JsonRequest request{effectiveId, type, data};
    const QByteArray frame = FrameCodec::encode(JsonProtocol::encodeRequest(request));
    if (frame.isEmpty() || socket_->write(frame) < 0) {
        emit networkError(QStringLiteral("failed to send request: %1")
                              .arg(socket_->errorString()));
        return QString();
    }
    if (isChargeLifecycleRoute(type)) {
        tracedChargeRequests_.insert(effectiveId);
        Logger::info(QStringLiteral("network-client"),
                     QStringLiteral("CLIENT SEND type=%1 request_id=%2")
                         .arg(type, effectiveId));
    }
    return effectiveId;
}

void NetworkClient::readResponses()
{
    QList<QByteArray> payloads;
    QString frameError;
    if (!codec_.append(socket_->readAll(), &payloads, &frameError)) {
        emit networkError(frameError);
        socket_->abort();
        return;
    }

    for (const QByteArray &payload : payloads) {
        JsonResponse response;
        ProtocolError error;
        if (!JsonProtocol::decodeResponse(payload, &response, &error)) {
            emit networkError(error.message);
            continue;
        }
        if (tracedChargeRequests_.remove(response.requestId) > 0) {
            Logger::info(QStringLiteral("network-client"),
                         QStringLiteral("CLIENT RECEIVE request_id=%1 success=%2 code=%3")
                             .arg(response.requestId)
                             .arg(response.success ? QStringLiteral("true")
                                                   : QStringLiteral("false"))
                             .arg(response.code));
        }
        emit responseReceived(response);
    }
}

}  // namespace ncs
