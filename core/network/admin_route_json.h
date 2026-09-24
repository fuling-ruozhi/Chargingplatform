#pragma once

#include "model/charger_status_summary.h"
#include "model/revenue_stats.h"

#include <QJsonObject>

namespace ncs {

QJsonObject revenueSummaryJson(const RevenueSummary &summary);
QJsonObject chargerStatusJson(const ChargerStatusSummary &summary);

}
