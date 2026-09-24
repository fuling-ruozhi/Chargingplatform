#include "geo_distance.h"

#include <QtMath>

namespace ncs {

bool GeoDistance::validCoordinate(double longitude, double latitude)
{
    return qIsFinite(longitude) && qIsFinite(latitude)
        && longitude >= -180.0 && longitude <= 180.0
        && latitude >= -90.0 && latitude <= 90.0;
}

double GeoDistance::haversineKm(double fromLongitude, double fromLatitude,
                                double toLongitude, double toLatitude)
{
    if (!validCoordinate(fromLongitude, fromLatitude)
        || !validCoordinate(toLongitude, toLatitude)) return 0.0;
    constexpr double earthRadiusKm = 6371.0;
    const double lat1 = qDegreesToRadians(fromLatitude);
    const double lat2 = qDegreesToRadians(toLatitude);
    const double deltaLat = qDegreesToRadians(toLatitude - fromLatitude);
    const double deltaLon = qDegreesToRadians(toLongitude - fromLongitude);
    const double a = qPow(qSin(deltaLat / 2.0), 2)
        + qCos(lat1) * qCos(lat2) * qPow(qSin(deltaLon / 2.0), 2);
    return earthRadiusKm * 2.0 * qAtan2(qSqrt(a), qSqrt(1.0 - a));
}

}
