#include "business_routes.h"

#include "json_protocol.h"
#include "request_router.h"
#include "service/session_manager.h"
#include "service/station_service.h"

#include <QJsonArray>

namespace ncs {
namespace {

QJsonObject recommendationStationJson(const Station &station)
{
    return {{QStringLiteral("id"), station.id},
            {QStringLiteral("name"), station.name},
            {QStringLiteral("address"), station.address},
            {QStringLiteral("price"), station.price},
            {QStringLiteral("total_slots"), station.totalSlots},
            {QStringLiteral("longitude"), station.longitude},
            {QStringLiteral("latitude"), station.latitude},
            {QStringLiteral("idle_slots"), station.idleSlots},
            {QStringLiteral("charger_count"), station.chargerCount},
            {QStringLiteral("distance_km"), station.distanceKm}};
}

QJsonObject stationRecommendationJson(const StationRecommendation &recommendation)
{
    QJsonArray reasons;
    for (const QString &reason : recommendation.reasons) reasons.append(reason);
    QJsonObject data = recommendationStationJson(recommendation.station);
    data.insert(QStringLiteral("score"), recommendation.score);
    data.insert(QStringLiteral("idle_rate"), recommendation.idleRate);
    data.insert(QStringLiteral("predicted_energy"), recommendation.predictedEnergy);
    data.insert(QStringLiteral("preference_orders"), recommendation.preferenceOrders);
    data.insert(QStringLiteral("preference_energy"), recommendation.preferenceEnergy);
    data.insert(QStringLiteral("weather_summary"), recommendation.weatherSummary);
    data.insert(QStringLiteral("reasons"), reasons);
    return data;
}

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

void BusinessRoutes::registerStationRecommendation(RequestRouter &router,
                                                   StationService &stationService,
                                                   SessionManager &sessionManager)
{
    router.registerHandler(QStringLiteral("station.recommend"), [&](const JsonRequest &request) {
        qint64 userId = 0;
        JsonResponse failure;
        if (!resolveUser(request, sessionManager, &userId, &failure)) return failure;
        const bool hasOrigin = request.data.value(QStringLiteral("longitude")).isDouble()
            && request.data.value(QStringLiteral("latitude")).isDouble();
        const double currentSoc = request.data.value(QStringLiteral("current_soc")).toDouble(50.0);
        const auto result = stationService.recommend(
            userId, request.data.value(QStringLiteral("longitude")).toDouble(),
            request.data.value(QStringLiteral("latitude")).toDouble(),
            hasOrigin, currentSoc);
        if (!result.success) {
            return JsonProtocol::failure(request.requestId,
                                         static_cast<int>(result.code), result.message);
        }
        QJsonArray items;
        for (const StationRecommendation &recommendation : result.value) {
            items.append(stationRecommendationJson(recommendation));
        }
        return JsonProtocol::success(request.requestId,
                                     {{QStringLiteral("recommendations"), items}});
    });
}

}
