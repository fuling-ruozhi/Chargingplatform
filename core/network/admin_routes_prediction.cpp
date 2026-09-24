// UC-A-08 管理端智能预测路由（NFR-M-01：与 admin_routes.cpp 拆分）
#include "admin_routes.h"
#include "json_protocol.h"
#include "model/load_prediction.h"
#include "request_router.h"
#include "service/admin_service.h"
#include "service/session_manager.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace ncs {
namespace {

bool resolveAdmin(const JsonRequest &request, SessionManager &sessions,
                  qint64 *adminId, JsonResponse *failure)
{
    BusinessErrorCode code = BusinessErrorCode::AuthRequired;
    QString message;
    if (sessions.resolveAdmin(
            request.data.value(QStringLiteral("admin_session_token")).toString(),
            adminId, &code, &message)) return true;
    *failure = JsonProtocol::failure(
        request.requestId, static_cast<int>(code), message);
    return false;
}

}

void AdminRoutes::registerPredictionRoutes(RequestRouter &router,
                                           AdminService &adminService,
                                           SessionManager &sessionManager)
{
    router.registerHandler(QStringLiteral("admin.prediction.list"),
                           [&](const JsonRequest &request) {
        qint64 adminId = 0; JsonResponse failure;
        if (!resolveAdmin(request, sessionManager, &adminId, &failure))
            return failure;
        const auto result = adminService.predictionList(adminId);
        if (!result.success)
            return JsonProtocol::failure(request.requestId,
                static_cast<int>(result.code), result.message);
        QJsonArray items;
        for (const LoadPrediction &prediction : result.value.items) {
            items.append(QJsonObject{
                {QStringLiteral("id"), prediction.id},
                {QStringLiteral("station_id"), prediction.stationId},
                {QStringLiteral("station_name"), prediction.stationName},
                {QStringLiteral("generated_at"), prediction.generatedAt},
                {QStringLiteral("target_time"), prediction.targetTime},
                {QStringLiteral("horizon_hours"), prediction.horizonHours},
                {QStringLiteral("predicted_energy"), prediction.predictedEnergy},
                {QStringLiteral("predicted_free_chargers"),
                 prediction.predictedFreeChargers},
                {QStringLiteral("is_peak"), prediction.isPeak}});
        }
        QJsonArray actual;
        for (const HourlyLoad &point : result.value.actual) {
            actual.append(QJsonObject{
                {QStringLiteral("station_id"), point.stationId},
                {QStringLiteral("station_name"), point.stationName},
                {QStringLiteral("time"), point.time},
                {QStringLiteral("energy"), point.energy}});
        }
        return JsonProtocol::success(request.requestId,
            {{QStringLiteral("generated_at"), result.value.generatedAt},
             {QStringLiteral("items"), items},
             {QStringLiteral("actual"), actual},
             {QStringLiteral("running"), result.value.runInProgress},
             {QStringLiteral("last_error"), result.value.lastRunError}});
    });
    // run 仅异步触发脚本并立即返回（幂等）；结果通过 admin.prediction.list 轮询，
    // 避免同步等待脚本期间阻塞单线程服务器的其他请求。
    router.registerHandler(QStringLiteral("admin.prediction.run"),
                           [&](const JsonRequest &request) {
        qint64 adminId = 0; JsonResponse failure;
        if (!resolveAdmin(request, sessionManager, &adminId, &failure))
            return failure;
        const auto result = adminService.runPrediction(adminId);
        return result.success
            ? JsonProtocol::success(request.requestId)
            : JsonProtocol::failure(request.requestId,
                static_cast<int>(result.code), result.message);
    });
}

}
