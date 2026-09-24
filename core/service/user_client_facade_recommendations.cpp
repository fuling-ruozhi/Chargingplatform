#include "user_client_facade.h"

#include <QJsonArray>

namespace ncs {

void UserClientFacade::requestStationRecommendations(
    double longitude, double latitude, double currentSocPercent)
{
    sendAuthenticated(QStringLiteral("station.recommend"),
                      {{QStringLiteral("longitude"), longitude},
                       {QStringLiteral("latitude"), latitude},
                       {QStringLiteral("current_soc"), currentSocPercent}});
}

bool UserClientFacade::handleStationRecommendationResponse(
    const JsonResponse &response, const QString &route)
{
    if (route != QStringLiteral("station.recommend")) return false;
    QVector<StationRecommendation> recommendations;
    for (const QJsonValue value
         : response.data.value(QStringLiteral("recommendations")).toArray()) {
        const QJsonObject object = value.toObject();
        StationRecommendation item;
        item.station = {object.value(QStringLiteral("id")).toInteger(),
                        object.value(QStringLiteral("name")).toString(),
                        object.value(QStringLiteral("address")).toString(),
                        object.value(QStringLiteral("price")).toDouble(),
                        object.value(QStringLiteral("total_slots")).toInt()};
        item.station.longitude = object.value(QStringLiteral("longitude")).toDouble();
        item.station.latitude = object.value(QStringLiteral("latitude")).toDouble();
        item.station.idleSlots = object.value(QStringLiteral("idle_slots")).toInt();
        item.station.chargerCount = object.value(QStringLiteral("charger_count")).toInt();
        item.station.distanceKm = object.value(QStringLiteral("distance_km")).toDouble();
        item.score = object.value(QStringLiteral("score")).toDouble();
        item.idleRate = object.value(QStringLiteral("idle_rate")).toDouble();
        item.predictedEnergy = object.value(QStringLiteral("predicted_energy")).toDouble();
        item.preferenceOrders = object.value(QStringLiteral("preference_orders")).toInt();
        item.preferenceEnergy = object.value(QStringLiteral("preference_energy")).toDouble();
        item.weatherSummary = object.value(QStringLiteral("weather_summary")).toString();
        for (const QJsonValue reason
             : object.value(QStringLiteral("reasons")).toArray()) {
            item.reasons.append(reason.toString());
        }
        recommendations.append(item);
    }
    emit stationRecommendationsReceived(recommendations);
    return true;
}

}
