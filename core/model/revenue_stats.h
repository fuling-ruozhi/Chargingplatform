#pragma once

#include <QDate>
#include <QMetaType>
#include <QString>
#include <QVector>

namespace ncs {

struct RevenueSummary
{
    double todayRevenue = 0.0;
    double monthRevenue = 0.0;
    double totalRevenue = 0.0;
};

struct RevenueDay
{
    QDate date;
    double revenue = 0.0;
    qint64 orderCount = 0;
};

struct RevenueTrend
{
    int days = 7;
    QVector<RevenueDay> items;
};

struct RecentOrder
{
    qint64 id = 0;
    QString chargerCode;
    QString stationName;
    QString startTime;
    QString endTime;
    double cost = 0.0;
};

using RecentOrders = QVector<RecentOrder>;

}

Q_DECLARE_METATYPE(ncs::RevenueSummary)
Q_DECLARE_METATYPE(ncs::RevenueTrend)
Q_DECLARE_METATYPE(ncs::RecentOrders)
