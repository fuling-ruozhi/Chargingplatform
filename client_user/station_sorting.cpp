#include "station_sorting.h"

#include <algorithm>

namespace ncs {

QVector<Station> sortStations(QVector<Station> stations, StationSortMode mode)
{
    std::stable_sort(stations.begin(), stations.end(), [mode](const Station &left,
                                                               const Station &right) {
        if (mode == StationSortMode::Recommendation) {
            if (left.recommendScore != right.recommendScore)
                return left.recommendScore > right.recommendScore;
            return left.distanceKm < right.distanceKm;
        }
        if (mode == StationSortMode::Rating) {
            const bool leftRated = left.reviewCount > 0;
            const bool rightRated = right.reviewCount > 0;
            if (leftRated != rightRated) return leftRated;
            if (left.averageScore != right.averageScore)
                return left.averageScore > right.averageScore;
            if (left.reviewCount != right.reviewCount)
                return left.reviewCount > right.reviewCount;
            return left.distanceKm < right.distanceKm;
        }
        return left.distanceKm < right.distanceKm;
    });
    return stations;
}

}
