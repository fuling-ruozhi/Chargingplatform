#include "database/database_manager.h"
#include "repository/charger_repository.h"
#include "repository/revenue_repository.h"
#include "service/charger_service.h"
#include "service/revenue_service.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QSqlQuery>
#include <QTemporaryDir>

namespace {

bool insertOrder(QSqlDatabase database, const QString &orderNo,
                 const QString &endTime, int status, double amount)
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO charging_order(order_no,user_id,charger_id,station_id,status,"
        "end_time,amount,created_at) VALUES(:order_no,1,1,1,:status,:end_time,"
        ":amount,:created_at)"));
    query.bindValue(QStringLiteral(":order_no"), orderNo);
    query.bindValue(QStringLiteral(":status"), status);
    query.bindValue(QStringLiteral(":end_time"), endTime);
    query.bindValue(QStringLiteral(":amount"), amount);
    query.bindValue(QStringLiteral(":created_at"), endTime);
    return query.exec();
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::DatabaseManager database(directory.filePath(QStringLiteral("analytics.db")));
    if (!database.initialize()) return 1;

    QSqlQuery clear(database.connection());
    if (!clear.exec(QStringLiteral("DELETE FROM charging_order"))) return 2;
    const QDateTime now(QDate(2026, 9, 8), QTime(12, 0), Qt::LocalTime);
    const QString current = now.toUTC().toString(Qt::ISODateWithMs);
    const QString previous = now.addDays(-8).toUTC().toString(Qt::ISODateWithMs);
    if (!insertOrder(database.connection(), QStringLiteral("A-1"), current, 2, 12.5)
        || !insertOrder(database.connection(), QStringLiteral("A-1B"), current, 2, 2.5)
        || !insertOrder(database.connection(), QStringLiteral("A-2"), previous, 2, 3.0)
        || !insertOrder(database.connection(), QStringLiteral("A-3"), current, 1, 999.0)) {
        return 3;
    }

    ncs::RevenueRepository revenueRepository(database);
    ncs::RevenueService revenueService(
        revenueRepository, [now] { return now; });
    const auto summary = revenueService.summary();
    if (!summary.success || summary.value.todayRevenue != 15.0
        || summary.value.monthRevenue != 15.0 || summary.value.totalRevenue != 18.0) {
        return 4;
    }
    const auto trend = revenueService.trend(7);
    if (!trend.success || trend.value.items.size() != 7
        || trend.value.items.last().orderCount != 2
        || trend.value.items.last().revenue != 15.0) return 5;
    const auto longTrend = revenueService.trend(30);
    if (!longTrend.success || longTrend.value.items.size() != 30
        || longTrend.value.items.last().orderCount != 2
        || longTrend.value.items.at(0).orderCount != 0) return 18;
    if (revenueService.trend(6).success) return 6;
    const auto recent = revenueService.recentOrders();
    if (!recent.success || recent.value.size() != 3) return 7;

    ncs::ChargerRepository chargerRepository(database);
    ncs::ChargerService chargerService(chargerRepository);
    const auto status = chargerService.statusSummary();
    if (!status.success || status.value.total != 40
        || status.value.idle + status.value.inUse + status.value.fault != 40
        || status.value.health < 0.0 || status.value.health > 100.0) return 8;
    return 0;
}
