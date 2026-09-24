#pragma once

#include "config/network_config.h"
#include "frame_codec.h"
#include "json_protocol.h"

#include <QObject>
#include <QSet>

class QTcpSocket;

namespace ncs {

class NetworkClient : public QObject
{
    Q_OBJECT

public:
    explicit NetworkClient(QObject *parent = nullptr);
    ~NetworkClient() override;

    bool isConnected() const;

public slots:
    void connectToServer(const QString &host = NetworkConfig::defaultHost(),
                         quint16 port = NetworkConfig::DefaultPort);
    void disconnectFromServer();
    QString sendRequest(const QString &type,
                        const QJsonObject &data = QJsonObject(),
                        const QString &requestId = QString());

signals:
    void connected();
    void disconnected();
    void responseReceived(const ncs::JsonResponse &response);
    void networkError(const QString &message);

private slots:
    void readResponses();

private:
    QTcpSocket *socket_ = nullptr;
    FrameCodec codec_;
    QSet<QString> tracedChargeRequests_;
};

}  // namespace ncs
