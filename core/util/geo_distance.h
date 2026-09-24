#pragma once

namespace ncs {

class GeoDistance
{
public:
    static bool validCoordinate(double longitude, double latitude);
    static double haversineKm(double fromLongitude, double fromLatitude,
                              double toLongitude, double toLatitude);
};

}
