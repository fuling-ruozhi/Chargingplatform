#include "admin_route_json.h"

namespace ncs {

QJsonObject revenueSummaryJson(const RevenueSummary &summary)
{
    return {{QStringLiteral("todayRevenue"), summary.todayRevenue},
            {QStringLiteral("monthRevenue"), summary.monthRevenue},
            {QStringLiteral("totalRevenue"), summary.totalRevenue}};
}

QJsonObject chargerStatusJson(const ChargerStatusSummary &summary)
{
    return {{QStringLiteral("total"), summary.total},
            {QStringLiteral("idle"), summary.idle},
            {QStringLiteral("in_use"), summary.inUse},
            {QStringLiteral("fault"), summary.fault},
            {QStringLiteral("health"), summary.health},
            {QStringLiteral("idle_percent"), summary.idlePercent},
            {QStringLiteral("in_use_percent"), summary.inUsePercent},
            {QStringLiteral("fault_percent"), summary.faultPercent}};
}

}
