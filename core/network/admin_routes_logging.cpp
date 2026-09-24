#include "admin_routes.h"
#include "json_protocol.h"
#include "request_router.h"
#include "service/admin_service.h"
#include "service/log_service.h"
#include "service/session_manager.h"
#include <QJsonArray>

namespace ncs {
namespace {
bool resolve(const JsonRequest &request, SessionManager &sessions, qint64 *id, JsonResponse *failure)
{
    BusinessErrorCode code; QString message;
    if (sessions.resolveAdmin(request.data.value("admin_session_token").toString(), id, &code, &message)) return true;
    *failure = JsonProtocol::failure(request.requestId, static_cast<int>(code), message); return false;
}
}
void registerLogRoutes(RequestRouter &router, AdminService &service, SessionManager &sessions)
{
    router.registerHandler("admin.logs.query", [&](const JsonRequest &request) {
        qint64 id = 0; JsonResponse failure;
        if (!resolve(request, sessions, &id, &failure)) return failure;
        Q_UNUSED(id)
        const QString kind = request.data.value("kind").toString();
        const QString keyword = request.data.value("keyword").toString().left(64);
        const QString type = request.data.value("type").toString().left(32);
        const QString result = request.data.value("result").toString().left(16);
        QJsonArray items;
        if (!service.logService()) return JsonProtocol::failure(request.requestId, 5000, "日志服务不可用");
        if (kind == "login") for (const auto &v : service.logService()->loginLogs(keyword, result)) items.append(QJsonObject{{"time",v.time},{"username",v.username},{"ip",v.ip},{"result",v.result},{"reason",v.reason},{"failed_attempts",v.failedAttempts}});
        else if (kind == "operation") for (const auto &v : service.logService()->operationLogs(keyword, type, result)) items.append(QJsonObject{{"time",v.time},{"username",v.username},{"module",v.module},{"action",v.action},{"target_type",v.targetType},{"target_id",v.targetId},{"detail",v.detail},{"result",v.result},{"error",v.errorMessage}});
        else if (kind == "security") for (const auto &v : service.logService()->securityLogs(type, result)) items.append(QJsonObject{{"time",v.time},{"severity",v.severity},{"event_type",v.eventType},{"username",v.username},{"ip",v.ip},{"description",v.description}});
        else {
            service.logService()->invalidParameter(id, QStringLiteral("admin.logs.query"),
                QStringLiteral("kind"), QStringLiteral("unknown log kind"));
            return JsonProtocol::failure(request.requestId,
                static_cast<int>(BusinessErrorCode::InvalidArgument), "日志类型无效");
        }
        return JsonProtocol::success(request.requestId, {{"items", items}});
    });
}
}
