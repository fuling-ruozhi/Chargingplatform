#pragma once

#include "model/review.h"

#include <QJsonObject>

namespace ncs {

QJsonObject reviewJson(const Review &review);
QJsonObject ratingSummaryJson(const StationRatingSummary &summary);

}
