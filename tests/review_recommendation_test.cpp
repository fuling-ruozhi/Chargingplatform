#include "model/review.h"
#include "model/station.h"
#include "service/station_rating_insight.h"
#include "service/station_recommendation.h"
#include "../client_user/station_sorting.h"

#include <cassert>
#include <cmath>

using namespace ncs;

int main()
{
    const StationRecommendationInput input{1.0, 10, 4.0, 3.5, 4, 8};
    const double expected = 2.5 * .35 + 4.0 * .30 + 2.5 * .25 + 3.5 * .10;
    assert(std::abs(calculateStationRecommendScore(input) - expected) < 0.0001);
    assert(calculateStationRecommendScore({1.0, 0, 0.0, 0.0, 4, 8}) > 0.0);
    assert(calculateStationRecommendScore({1.0, 1, 5.0, 5.0, 8, 8})
           > calculateStationRecommendScore({1.0, 1, 5.0, 5.0, 1, 8}));
    assert(calculateStationRecommendScore({0.0, 1, 5.0, 5.0, 8, 8})
           > calculateStationRecommendScore({10.0, 1, 5.0, 5.0, 8, 8}));
    assert(calculateStationRecommendScore({0.0, 1, 99.0, 99.0, 8, 8}) <= 5.0);

    Station nearRated; nearRated.distanceKm = 1.0; nearRated.reviewCount = 4;
    nearRated.averageScore = 4.0; nearRated.recommendScore = 4.0;
    Station unrated; unrated.distanceKm = 0.1; unrated.recommendScore = 3.0;
    Station farRated = nearRated; farRated.distanceKm = 2.0; farRated.reviewCount = 8;
    QVector<Station> stations{unrated, farRated, nearRated};
    const auto rating = sortStations(stations, StationSortMode::Rating);
    assert(rating.at(0).reviewCount == 8 && rating.at(1).reviewCount == 4);
    const auto distance = sortStations(stations, StationSortMode::Distance);
    assert(distance.at(0).distanceKm == 0.1);

    StationRatingSummary insufficient;
    assert(stationRatingInsights(insufficient).at(0).message == QStringLiteral("评价样本不足"));
    StationRatingSummary warning{3, 3.4, 4.0, 2.0, 2.5, 4.0};
    const auto warnings = stationRatingInsights(warning);
    assert(warnings.size() == 2);
    StationRatingSummary normal{3, 4.2, 4.0, 4.0, 4.0, 4.0};
    assert(stationRatingInsights(normal).at(0).message == QStringLiteral("评价状态正常"));
    return 0;
}
