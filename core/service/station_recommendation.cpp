#include "station_recommendation.h"

#include <QtGlobal>

namespace ncs {

double calculateStationRecommendScore(const StationRecommendationInput &input)
{
    // Keep the weights transparent: distance 35%, rating 30%, availability 25%, queue 10%.
    const double distanceScore = qBound(0.0, 5.0 / (1.0 + qMax(0.0, input.distanceKm)), 5.0);
    const double ratingScore = input.reviewCount > 0 ? qBound(0.0, input.averageScore, 5.0) : 3.0;
    const double availabilityScore = input.totalChargers > 0
        ? qBound(0.0, 5.0 * input.idleChargers / input.totalChargers, 5.0) : 0.0;
    const double queueScore = input.reviewCount > 0 ? qBound(0.0, input.queueScore, 5.0) : 3.0;
    return qBound(0.0, distanceScore * 0.35 + ratingScore * 0.30
                         + availabilityScore * 0.25 + queueScore * 0.10, 5.0);
}

}
