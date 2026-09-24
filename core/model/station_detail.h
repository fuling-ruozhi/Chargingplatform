#pragma once

#include "charger.h"
#include "review.h"
#include "station.h"

#include <QMetaType>

namespace ncs {

struct StationDetail
{
    Station station;
    QVector<Charger> chargers;
    StationRatingSummary ratingSummary;
};

}

Q_DECLARE_METATYPE(ncs::StationDetail)
