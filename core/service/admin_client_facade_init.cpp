#include "admin_client_facade.h"

namespace ncs {

AdminClientFacade::AdminClientFacade(QObject *parent) : QObject(parent), client_(this)
{
    qRegisterMetaType<ncs::Admin>();
    qRegisterMetaType<ncs::AdminSummary>();
    qRegisterMetaType<QVector<ncs::Station>>();
    connect(&client_, &NetworkClient::connected,
            this, &AdminClientFacade::connected);
    connect(&client_, &NetworkClient::disconnected, this, [this] {
        emit disconnected();
        failPending(QStringLiteral("网络连接已断开"));
    });
    connect(&client_, &NetworkClient::networkError, this, [this](const QString &message) {
        emit networkError(message);
        failPending(message);
    });
    connect(&client_, &NetworkClient::responseReceived,
            this, &AdminClientFacade::handle);
}

}
