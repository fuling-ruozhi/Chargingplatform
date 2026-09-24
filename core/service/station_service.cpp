#include "station_service.h"

#include "repository/station_repository.h"
#include "repository/review_repository.h"
#include "util/geo_distance.h"
#include "service/station_recommendation.h"

#include <QDate>

#include <algorithm>
#include <cmath>

namespace ncs {
namespace {

double clamp01(double value)
{
    return qBound(0.0, value, 1.0);
}

double normalizeLowerBetter(double value, double minValue, double maxValue)
{
    if (maxValue <= minValue) return 1.0;
    return clamp01(1.0 - (value - minValue) / (maxValue - minValue));
}

double normalizeHigherBetter(double value, double minValue, double maxValue)
{
    if (maxValue <= minValue) return 1.0;
    return clamp01((value - minValue) / (maxValue - minValue));
}

struct WeatherFactor
{
    double score = 1.0;
    QString summary;
};

WeatherFactor localWeatherFor(const Station &station)
{
    const int day = QDate::currentDate().dayOfYear();
    const int bucket = (static_cast<int>(std::abs(station.longitude * 10.0))
        + static_cast<int>(std::abs(station.latitude * 10.0)) + day) % 10;
    if (bucket <= 1) return {0.78, QStringLiteral("模拟天气：降雨，优先选择近且空闲的站")};
    if (bucket >= 8) return {0.86, QStringLiteral("模拟天气：高温，排队风险略高")};
    return {1.0, QStringLiteral("模拟天气：正常")};
}

QString percentText(double value)
{
    return QStringLiteral("%1%").arg(value * 100.0, 0, 'f', 0);
}

}

StationService::StationService(StationRepository &repository, ReviewRepository *reviewRepository)
    : repository_(repository), reviewRepository_(reviewRepository) {}

ServiceResult<QVector<Station>> StationService::list(double longitude, double latitude,
                                                     bool hasOrigin) const
{
    if (hasOrigin && !GeoDistance::validCoordinate(longitude, latitude)) {
        return ServiceResult<QVector<Station>>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("当前位置坐标无效"));
    }
    QVector<Station> stations;
    QString error;
    if (!repository_.list(&stations, &error)) {
        return ServiceResult<QVector<Station>>::fail(BusinessErrorCode::DatabaseError, error);
    }
    for (Station &station : stations) {
        station.recommendScore = calculateStationRecommendScore({
            0.0, station.reviewCount, station.averageScore, station.queueScore,
            station.idleSlots, station.chargerCount});
    }
    if (hasOrigin) {
        for (Station &station : stations) {
            station.distanceKm = GeoDistance::haversineKm(
                longitude, latitude, station.longitude, station.latitude);
            station.recommendScore = calculateStationRecommendScore({
                station.distanceKm, station.reviewCount, station.averageScore,
                station.queueScore, station.idleSlots, station.chargerCount});
        }
        std::sort(stations.begin(), stations.end(), [](const Station &left,
                                                       const Station &right) {
            return left.distanceKm == right.distanceKm
                ? left.id < right.id : left.distanceKm < right.distanceKm;
        });
    }
    return ServiceResult<QVector<Station>>::ok(stations);
}

ServiceResult<StationDetail> StationService::detail(qint64 stationId, double longitude,
                                                    double latitude, bool hasOrigin) const
{
    if (stationId <= 0
        || (hasOrigin && !GeoDistance::validCoordinate(longitude, latitude))) {
        return ServiceResult<StationDetail>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("电站编号无效"));
    }
    StationDetail value;
    bool found = false;
    QString error;
    if (!repository_.detail(stationId, &value, &found, &error)) {
        return ServiceResult<StationDetail>::fail(BusinessErrorCode::DatabaseError, error);
    }
    if (!found) {
        return ServiceResult<StationDetail>::fail(
            BusinessErrorCode::StationNotFound, QStringLiteral("电站不存在"));
    }
    if (reviewRepository_
        && !reviewRepository_->getStationRatingSummary(
            stationId, &value.ratingSummary, &error)) {
        return ServiceResult<StationDetail>::fail(
            BusinessErrorCode::DatabaseError, error);
    }
    if (hasOrigin) {
        value.station.distanceKm = GeoDistance::haversineKm(
            longitude, latitude, value.station.longitude, value.station.latitude);
    }
    return ServiceResult<StationDetail>::ok(value);
}

ServiceResult<QVector<StationRecommendation>> StationService::recommend(
    qint64 userId, double longitude, double latitude, bool hasOrigin,
    double currentSocPercent) const
{
    if (userId <= 0
        || (hasOrigin && !GeoDistance::validCoordinate(longitude, latitude))
        || !std::isfinite(currentSocPercent)
        || currentSocPercent < 0.0 || currentSocPercent > 100.0) {
        return ServiceResult<QVector<StationRecommendation>>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("推荐参数无效"));
    }

    QVector<Station> stations;
    QHash<qint64, StationPreferenceStats> preferences;
    QHash<qint64, double> predictions;
    QString error;
    if (!repository_.list(&stations, &error)
        || !repository_.userPreferenceStats(userId, &preferences, &error)
        || !repository_.latestPredictionEnergy(&predictions, &error)) {
        return ServiceResult<QVector<StationRecommendation>>::fail(
            BusinessErrorCode::DatabaseError, error);
    }
    if (stations.isEmpty()) {
        return ServiceResult<QVector<StationRecommendation>>::ok({});
    }

    double minDistance = 0.0;
    double maxDistance = 0.0;
    bool firstDistance = true;
    double minPrice = stations.first().price;
    double maxPrice = stations.first().price;
    double maxPreferenceOrders = 0.0;
    double minPrediction = 0.0;
    double maxPrediction = 0.0;
    bool firstPrediction = true;
    for (Station &station : stations) {
        if (hasOrigin) {
            station.distanceKm = GeoDistance::haversineKm(
                longitude, latitude, station.longitude, station.latitude);
        }
        if (firstDistance) {
            minDistance = maxDistance = station.distanceKm;
            firstDistance = false;
        } else {
            minDistance = qMin(minDistance, station.distanceKm);
            maxDistance = qMax(maxDistance, station.distanceKm);
        }
        minPrice = qMin(minPrice, station.price);
        maxPrice = qMax(maxPrice, station.price);
        maxPreferenceOrders = qMax(maxPreferenceOrders,
            static_cast<double>(preferences.value(station.id).completedOrders));
        if (predictions.contains(station.id)) {
            const double prediction = predictions.value(station.id);
            if (firstPrediction) {
                minPrediction = maxPrediction = prediction;
                firstPrediction = false;
            } else {
                minPrediction = qMin(minPrediction, prediction);
                maxPrediction = qMax(maxPrediction, prediction);
            }
        }
    }

    const bool lowSoc = currentSocPercent < 30.0;
    const bool highSoc = currentSocPercent >= 70.0;
    const double distanceWeight = lowSoc ? 0.26 : (highSoc ? 0.16 : 0.20);
    const double idleWeight = lowSoc ? 0.28 : (highSoc ? 0.20 : 0.24);
    const double priceWeight = lowSoc ? 0.14 : (highSoc ? 0.24 : 0.18);
    const double preferenceWeight = lowSoc ? 0.10 : (highSoc ? 0.18 : 0.13);
    const double predictionWeight = lowSoc ? 0.12 : (highSoc ? 0.12 : 0.15);
    const double weatherWeight = 0.10;

    QVector<StationRecommendation> recommendations;
    for (const Station &station : stations) {
        StationRecommendation item;
        item.station = station;
        item.idleRate = station.chargerCount > 0
            ? clamp01(static_cast<double>(station.idleSlots) / station.chargerCount)
            : 0.0;
        const bool hasPrediction = predictions.contains(station.id);
        item.predictedEnergy = hasPrediction ? predictions.value(station.id) : 0.0;
        const auto preference = preferences.value(station.id);
        item.preferenceOrders = preference.completedOrders;
        item.preferenceEnergy = preference.energyKwh;
        const WeatherFactor weather = localWeatherFor(station);
        item.weatherSummary = weather.summary;

        const double distanceScore = hasOrigin
            ? normalizeLowerBetter(station.distanceKm, minDistance, maxDistance)
            : 0.75;
        const double priceScore = normalizeLowerBetter(station.price, minPrice, maxPrice);
        const double preferenceScore = maxPreferenceOrders > 0.0
            ? normalizeHigherBetter(item.preferenceOrders, 0.0, maxPreferenceOrders)
            : 0.5;
        const double predictionScore = hasPrediction
            ? normalizeLowerBetter(item.predictedEnergy, minPrediction, maxPrediction)
            : 0.55;
        item.score = 100.0 * (
            distanceWeight * distanceScore
            + idleWeight * item.idleRate
            + priceWeight * priceScore
            + preferenceWeight * preferenceScore
            + predictionWeight * predictionScore
            + weatherWeight * weather.score);

        item.reasons.append(QStringLiteral("距离 %1 km").arg(station.distanceKm, 0, 'f', 2));
        item.reasons.append(QStringLiteral("空闲率 %1").arg(percentText(item.idleRate)));
        item.reasons.append(QStringLiteral("电价 ¥%1/kWh").arg(station.price, 0, 'f', 2));
        if (item.preferenceOrders > 0) {
            item.reasons.append(QStringLiteral("你历史在本站充过 %1 次")
                                    .arg(item.preferenceOrders));
        } else {
            item.reasons.append(QStringLiteral("无历史偏好，按综合条件推荐"));
        }
        if (hasPrediction) {
            item.reasons.append(QStringLiteral("预测 24h 负荷 %1 kWh")
                                    .arg(item.predictedEnergy, 0, 'f', 1));
        }
        item.reasons.append(item.weatherSummary);
        if (lowSoc) {
            item.reasons.append(QStringLiteral("当前 SoC 较低，距离和空闲权重更高"));
        } else if (highSoc) {
            item.reasons.append(QStringLiteral("当前 SoC 较高，价格和历史偏好权重更高"));
        }
        recommendations.append(item);
    }
    std::sort(recommendations.begin(), recommendations.end(),
              [](const StationRecommendation &left,
                 const StationRecommendation &right) {
        if (std::abs(left.score - right.score) > 1e-9) return left.score > right.score;
        return left.station.id < right.station.id;
    });
    if (recommendations.size() > 3) recommendations.resize(3);
    return ServiceResult<QVector<StationRecommendation>>::ok(recommendations);
}

}
