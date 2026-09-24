#include "request_router.h"

#include "config/network_config.h"

#include <QDateTime>

namespace ncs {

RequestRouter::RequestRouter()
{
    registerHandler(QStringLiteral("system.ping"), [](const JsonRequest &request) {
        QJsonObject data;
        data.insert(QStringLiteral("pong"), true);
        data.insert(QStringLiteral("server_time"),
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        data.insert(QStringLiteral("protocol_version"), NetworkConfig::ProtocolVersion);
        return JsonProtocol::success(request.requestId, data);
    });
}

void RequestRouter::registerHandler(const QString &type, Handler handler)
{
    handlers_.insert(type, std::move(handler));
}

JsonResponse RequestRouter::route(const JsonRequest &request) const
{
    const auto handler = handlers_.constFind(request.type);
    if (handler == handlers_.constEnd()) {
        return JsonProtocol::failure(request.requestId,
                                     ProtocolErrorCode::UnknownRequestType,
                                     QStringLiteral("unknown request type"));
    }
    return handler.value()(request);
}

}  // namespace ncs
