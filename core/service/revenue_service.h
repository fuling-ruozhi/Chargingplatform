#pragma once

#include "model/revenue_stats.h"
#include "model/service_result.h"

#include <QDateTime>
#include <functional>

namespace ncs {

class RevenueRepository;

class RevenueService
{
public:
    using Clock = std::function<QDateTime()>;

    explicit RevenueService(RevenueRepository &repository,
                            Clock clock = [] { return QDateTime::currentDateTime(); });

    ServiceResult<RevenueSummary> summary() const;
    ServiceResult<RevenueTrend> trend(int days) const;
    ServiceResult<RecentOrders> recentOrders() const;

private:
    RevenueRepository &repository_;
    Clock clock_;
};

}
