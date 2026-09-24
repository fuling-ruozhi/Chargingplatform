#include "revenue_repository.h"

#include "database/database_manager.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QMap>

namespace ncs {
namespace {

QString utc(const QDateTime &value)
{
    return value.toUTC().toString(Qt::ISODateWithMs);
}

bool execute(QSqlQuery &query, QString *error)
{
    if (query.exec()) {
        return true;
    }
    *error = query.lastError().text();
    return false;
}

}

bool RevenueRepository::summary(const QDateTime &today, const QDateTime &month,
                                const QDateTime &now, RevenueSummary *result,
                                QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT COALESCE(SUM(CASE WHEN status=:completed "
            "AND end_time IS NOT NULL AND julianday(end_time)>=julianday(:today) "
            "AND julianday(end_time)<=julianday(:now) THEN amount ELSE 0 END),0),"
            "COALESCE(SUM(CASE WHEN status=:completed AND end_time IS NOT NULL "
            "AND julianday(end_time)>=julianday(:month) "
            "AND julianday(end_time)<=julianday(:now) THEN amount ELSE 0 END),0),"
            "COALESCE(SUM(CASE WHEN status=:completed AND end_time IS NOT NULL "
            "THEN amount ELSE 0 END),0) FROM charging_order"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":completed"), 2);
    query.bindValue(QStringLiteral(":today"), utc(today));
    query.bindValue(QStringLiteral(":month"), utc(month));
    query.bindValue(QStringLiteral(":now"), utc(now));
    if (!execute(query, error) || !query.next()) {
        if (error->isEmpty()) {
            *error = QStringLiteral("Missing revenue aggregate");
        }
        return false;
    }
    result->todayRevenue = query.value(0).toDouble();
    result->monthRevenue = query.value(1).toDouble();
    result->totalRevenue = query.value(2).toDouble();
    return true;
}

bool RevenueRepository::daily(const QVector<QDateTime> &boundaries,
                              const QDateTime &now, QVector<RevenueDay> *result,
                              QString *error) const
{
    result->clear();
    if (boundaries.size() < 2) return true;

    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT date(end_time,'localtime'),COALESCE(SUM(amount),0),COUNT(*) "
            "FROM charging_order "
            "WHERE status=:completed AND end_time IS NOT NULL "
            "AND julianday(end_time)>=julianday(:begin) "
            "AND julianday(end_time)<julianday(:end) "
            "AND julianday(end_time)<=julianday(:now) "
            "GROUP BY date(end_time,'localtime') ORDER BY date(end_time,'localtime')"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":completed"), 2);
    query.bindValue(QStringLiteral(":begin"), utc(boundaries.first()));
    query.bindValue(QStringLiteral(":end"), utc(boundaries.last()));
    query.bindValue(QStringLiteral(":now"), utc(now));
    if (!execute(query, error)) return false;

    QMap<QDate, RevenueDay> aggregate;
    while (query.next()) {
        const QDate date = QDate::fromString(query.value(0).toString(), Qt::ISODate);
        if (date.isValid()) {
            aggregate.insert(date, {date, query.value(1).toDouble(),
                                    query.value(2).toLongLong()});
        }
    }
    for (int index = 0; index + 1 < boundaries.size(); ++index) {
        const QDate date = boundaries.at(index).date();
        result->append(aggregate.value(date, {date, 0.0, 0}));
    }
    return true;
}

bool RevenueRepository::recent(int limit, RecentOrders *result, QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT o.id,c.code,s.name,o.start_time,o.end_time,o.amount "
            "FROM charging_order o JOIN charger c ON c.id=o.charger_id "
            "JOIN station s ON s.id=o.station_id "
            "WHERE o.status=:completed AND o.end_time IS NOT NULL "
            "ORDER BY julianday(o.end_time) DESC,o.id DESC LIMIT :limit"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":completed"), 2);
    query.bindValue(QStringLiteral(":limit"), limit);
    if (!execute(query, error)) {
        return false;
    }
    result->clear();
    while (query.next()) {
        result->append({query.value(0).toLongLong(), query.value(1).toString(),
                        query.value(2).toString(), query.value(3).toString(),
                        query.value(4).toString(), query.value(5).toDouble()});
    }
    return true;
}

}
