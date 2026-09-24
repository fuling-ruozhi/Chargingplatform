// EXT-SC-03 user.forecast 路由（NFR-M-01：与 business_routes.cpp 拆分）
#include "business_routes.h"
#include "json_protocol.h"
#include "model/charge_forecast.h"
#include "request_router.h"
#include "service/charge_forecast_service.h"
#include "service/session_manager.h"

#include <QJsonObject>

namespace ncs {
namespace {

bool resolveUser(const JsonRequest &request, SessionManager &sessions,
                 qint64 *userId, JsonResponse *failure)
{
    BusinessErrorCode code = BusinessErrorCode::AuthRequired;
    QString message;
    if (sessions.resolveUser(
            request.data.value(QStringLiteral("session_token")).toString(),
            userId, &code, &message)) return true;
    *failure = JsonProtocol::failure(
        request.requestId, static_cast<int>(code), message);
    return false;
}

}

void BusinessRoutes::registerForecastRoute(RequestRouter &router,
                                           ChargeForecastService &forecastService,
                                           SessionManager &sessionManager)
{
    router.registerHandler(QStringLiteral("user.forecast"),
                           [&](const JsonRequest &request) {
        qint64 userId = 0;
        JsonResponse failure;
        if (!resolveUser(request, sessionManager, &userId, &failure))
            return failure;
        const auto result = forecastService.forecast(userId);
        if (!result.success)
            return JsonProtocol::failure(request.requestId,
                static_cast<int>(result.code), result.message);
        const ChargeForecast &forecast = result.value;
        return JsonProtocol::success(request.requestId,
            {{QStringLiteral("charging"), forecast.charging},
             {QStringLiteral("cold_start"), forecast.coldStart},
             {QStringLiteral("current_soc"), forecast.currentSoc},
             {QStringLiteral("daily_kwh"), forecast.dailyKwh},
             {QStringLiteral("remaining_days"), forecast.remainingDays},
             {QStringLiteral("suggested_date"), forecast.suggestedDate},
             {QStringLiteral("method"), forecast.method},
             {QStringLiteral("note"), forecast.note}});
    });
}

}
