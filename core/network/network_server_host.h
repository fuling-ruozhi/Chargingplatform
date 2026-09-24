#pragma once

#include "config/network_config.h"

#include <QObject>
#include <QThread>

namespace ncs {

class NetworkServer;

class NetworkServerHost : public QObject
{
    Q_OBJECT

public:
    explicit NetworkServerHost(const QString &databasePath = QString(),
                               QObject *parent = nullptr);
    ~NetworkServerHost() override;

    void startServer(const QString &host = NetworkConfig::defaultHost(),
                     quint16 port = NetworkConfig::DefaultPort);
    void stopServer();
    void shutdown();

signals:
    void startRequested(const QString &host, quint16 port);
    void serverStarted(quint16 port);
    void serverStopped();
    void serverError(const QString &message);
    void databaseError(const QString &message);
    void protocolError(const QString &message);
    void clientConnected(const QString &peer);
    void clientDisconnected(const QString &peer);

private:
    QThread serverThread_;
    NetworkServer *server_ = nullptr;
};

}  // namespace ncs
