#pragma once

#include "model/revenue_stats.h"

#include <QDateTime>
#include <QVector>

namespace ncs {

class DatabaseManager;

class RevenueRepository
{
public:
    explicit RevenueRepository(DatabaseManager &database) : database_(database) {}

    bool summary(const QDateTime &today, const QDateTime &month,
                 const QDateTime &now, RevenueSummary *result,
                 QString *error) const;
    bool daily(const QVector<QDateTime> &boundaries, const QDateTime &now,
               QVector<RevenueDay> *result, QString *error) const;
    bool recent(int limit, RecentOrders *result, QString *error) const;

private:
    DatabaseManager &database_;
};

}
