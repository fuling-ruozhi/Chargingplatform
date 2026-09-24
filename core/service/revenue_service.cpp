#include "revenue_service.h"

#include "repository/revenue_repository.h"

#include <utility>

namespace ncs {

RevenueService::RevenueService(RevenueRepository &repository, Clock clock)
    : repository_(repository), clock_(std::move(clock))
{
}

ServiceResult<RevenueSummary> RevenueService::summary() const
{
    const QDateTime now = clock_().toLocalTime();
    const QDateTime today = now.date().startOfDay();
    const QDateTime month = QDate(now.date().year(), now.date().month(), 1).startOfDay();
    RevenueSummary result;
    QString error;
    if (!repository_.summary(today, month, now, &result, &error)) {
        return ServiceResult<RevenueSummary>::fail(
            BusinessErrorCode::DatabaseError, error);
    }
    return ServiceResult<RevenueSummary>::ok(result);
}

ServiceResult<RevenueTrend> RevenueService::trend(int days) const
{
    if (days != 7 && days != 30) {
        return ServiceResult<RevenueTrend>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("days 仅支持 7 或 30"));
    }
    const QDateTime now = clock_().toLocalTime();
    QVector<QDateTime> boundaries;
    for (int index = 1 - days; index <= 1; ++index) {
        boundaries.append(now.date().addDays(index).startOfDay());
    }
    RevenueTrend result;
    result.days = days;
    QString error;
    if (!repository_.daily(boundaries, now, &result.items, &error)) {
        return ServiceResult<RevenueTrend>::fail(
            BusinessErrorCode::DatabaseError, error);
    }
    return ServiceResult<RevenueTrend>::ok(result);
}

ServiceResult<RecentOrders> RevenueService::recentOrders() const
{
    RecentOrders result;
    QString error;
    if (!repository_.recent(10, &result, &error)) {
        return ServiceResult<RecentOrders>::fail(
            BusinessErrorCode::DatabaseError, error);
    }
    return ServiceResult<RecentOrders>::ok(result);
}

}
