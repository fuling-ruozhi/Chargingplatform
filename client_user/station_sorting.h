#pragma once

#include "model/station.h"

namespace ncs {

enum class StationSortMode { Distance, Recommendation, Rating };

QVector<Station> sortStations(QVector<Station> stations, StationSortMode mode);

}
