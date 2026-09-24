#pragma once

#include "model/review.h"

#include <QVector>

namespace ncs {

struct StationRatingInsight
{
    QString type;
    QString level;
    QString message;
};

QVector<StationRatingInsight> stationRatingInsights(const StationRatingSummary &summary);

}
