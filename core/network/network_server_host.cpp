#include "network_server_host.h"

#include "network_server.h"

#include <QMetaObject>

namespace ncs {

NetworkServerHost::NetworkServerHost(const QString &databasePath, QObject *parent)
    : QObject(parent), server_(new NetworkServer(databasePath))
{
    server_->moveToThread(&serverThread_);
    connect(this, &NetworkServerHost::startRequested,
            server_, &NetworkServer::startServer, Qt::QueuedConnection);
    connect(server_, &NetworkServer::started,
            this, &NetworkServerHost::serverStarted);
    connect(server_, &NetworkServer::stopped,
            this, &NetworkServerHost::serverStopped);
    connect(server_, &NetworkServer::serverError,
            this, &NetworkServerHost::serverError);
    connect(server_, &NetworkServer::databaseError,
            this, &NetworkServerHost::databaseError);
    connect(server_, &NetworkServer::protocolError,
            this, &NetworkServerHost::protocolError);
    connect(server_, &NetworkServer::clientConnected,
            this, &NetworkServerHost::clientConnected);
    connect(server_, &NetworkServer::clientDisconnected,
            this, &NetworkServerHost::clientDisconnected);
    connect(&serverThread_, &QThread::finished, server_, &QObject::deleteLater);
    serverThread_.setObjectName(QStringLiteral("NCS Network Server Thread"));
    serverThread_.start();
}

NetworkServerHost::~NetworkServerHost()
{
    shutdown();
}

void NetworkServerHost::startServer(const QString &host, quint16 port)
{
    QMetaObject::invokeMethod(this, [this, host, port] {
        if (server_) emit startRequested(host, port);
    }, Qt::QueuedConnection);
}

void NetworkServerHost::stopServer()
{
    if (!serverThread_.isRunning() || !server_) {
        return;
    }
    if (QThread::currentThread() == server_->thread()) {
        server_->stopServer();
    } else {
        QMetaObject::invokeMethod(server_, "stopServer", Qt::BlockingQueuedConnection);
    }
}

void NetworkServerHost::shutdown()
{
    if (!serverThread_.isRunning()) {
        return;
    }
    stopServer();
    serverThread_.quit();
    serverThread_.wait();
    server_ = nullptr;
}

}  // namespace ncs
