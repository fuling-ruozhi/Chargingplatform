#pragma once

#include <QtGlobal>

namespace ncs {

struct StationRecommendationInput
{
    double distanceKm = 0.0;
    qint64 reviewCount = 0;
    double averageScore = 0.0;
    double queueScore = 0.0;
    int idleChargers = 0;
    int totalChargers = 0;
};

double calculateStationRecommendScore(const StationRecommendationInput &input);

}
