#include "charger_repository.h"

#include "database/database_manager.h"
#include "model/charger.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace ncs {
namespace {

QString chargerProjection()
{
    return QStringLiteral(
        "c.id,c.station_id,s.name,c.code,c.status,"
        "COALESCE(r.status,-1),c.type,c.power_kw,c.total_count,c.total_minutes");
}

Charger chargerFromQuery(const QSqlQuery &query)
{
    Charger charger;
    charger.id = query.value(0).toLongLong();
    charger.stationId = query.value(1).toLongLong();
    charger.stationName = query.value(2).toString();
    charger.code = query.value(3).toString();
    charger.status = static_cast<ChargerStatus>(query.value(4).toInt());
    charger.activeOrderStatus = query.value(5).toInt();
    charger.type = query.value(6).toInt();
    charger.powerKw = query.value(7).toDouble();
    charger.totalCount = query.value(8).toInt();
    charger.totalMinutes = query.value(9).toLongLong();
    return charger;
}

}

bool ChargerRepository::statusSummary(ChargerStatusSummary *result,
                                       QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT COUNT(*),COALESCE(SUM(status=:idle),0),"
            "COALESCE(SUM(status=:using),0),COALESCE(SUM(status=:fault),0) "
            "FROM charger"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":idle"), static_cast<int>(ChargerStatus::Idle));
    query.bindValue(QStringLiteral(":using"), static_cast<int>(ChargerStatus::Using));
    query.bindValue(QStringLiteral(":fault"), static_cast<int>(ChargerStatus::Fault));
    if (!query.exec() || !query.next()) {
        *error = query.lastError().text();
        return false;
    }
    result->total = query.value(0).toLongLong();
    result->idle = query.value(1).toLongLong();
    result->inUse = query.value(2).toLongLong();
    result->fault = query.value(3).toLongLong();
    return true;
}

bool ChargerRepository::list(const QString &keyword, int status,
                             QVector<Charger> *result, QString *error, qint64 stationId) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT %1 "
            "FROM charger c JOIN station s ON s.id=c.station_id "
            "LEFT JOIN charging_order r ON r.charger_id=c.id AND r.status IN(0,1) "
            "WHERE (:keyword='' OR c.code LIKE :pattern OR s.name LIKE :pattern) "
            "AND (:status<0 OR c.status=:status) AND (:station_id<0 OR c.station_id=:station_id) "
            "ORDER BY c.id").arg(chargerProjection()))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":keyword"), keyword);
    query.bindValue(QStringLiteral(":pattern"), QStringLiteral("%") + keyword + "%");
    query.bindValue(QStringLiteral(":status"), status);
    query.bindValue(QStringLiteral(":station_id"), stationId);
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    result->clear();
    while (query.next()) {
        result->append(chargerFromQuery(query));
    }
    return true;
}

bool ChargerRepository::insertBatch(qint64 stationId, const QStringList &codes, int type,
                                     double powerKw, QVector<Charger> *result,
                                     QString *error) const
{
    result->clear();
    if (!database_.transaction()) { *error = database_.lastError(); return false; }
    for (const QString &code : codes) {
        Charger charger;
        if (!insert(stationId, code, type, powerKw, &charger, error)) {
            database_.rollback();
            return false;
        }
        result->append(charger);
    }
    if (!database_.commit()) {
        *error = database_.lastError();
        database_.rollback();
        return false;
    }
    return true;
}

bool ChargerRepository::findById(qint64 id, Charger *result, bool *found,
                                 QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT %1 FROM charger c JOIN station s ON s.id=c.station_id "
            "LEFT JOIN charging_order r ON r.charger_id=c.id AND r.status IN(0,1) "
            "WHERE c.id=:id LIMIT 1").arg(chargerProjection()))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    *found = query.next();
    if (*found) *result = chargerFromQuery(query);
    return true;
}

bool ChargerRepository::stationExists(qint64 stationId, bool *found,
                                       QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral("SELECT 1 FROM station WHERE id=:id"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":id"), stationId);
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    *found = query.next();
    return true;
}

bool ChargerRepository::insert(qint64 stationId, const QString &code, int type,
                               double powerKw, Charger *result, QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "INSERT INTO charger(station_id,code,type,power_kw,status) "
            "VALUES(:station_id,:code,:type,:power_kw,:status)"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":station_id"), stationId);
    query.bindValue(QStringLiteral(":code"), code);
    query.bindValue(QStringLiteral(":type"), type);
    query.bindValue(QStringLiteral(":power_kw"), powerKw);
    query.bindValue(QStringLiteral(":status"), static_cast<int>(ChargerStatus::Idle));
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    bool found = false;
    return findById(query.lastInsertId().toLongLong(), result, &found, error) && found;
}

bool ChargerRepository::remove(qint64 id, bool *removed, QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral("DELETE FROM charger WHERE id=:id"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    *removed = query.numRowsAffected() == 1;
    return true;
}

bool ChargerRepository::updateStatus(qint64 id, ChargerStatus expected,
                                      ChargerStatus next, bool *updated,
                                      QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "UPDATE charger SET status=:next WHERE id=:id AND status=:expected"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":next"), static_cast<int>(next));
    query.bindValue(QStringLiteral(":id"), id);
    query.bindValue(QStringLiteral(":expected"), static_cast<int>(expected));
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    *updated = query.numRowsAffected() == 1;
    return true;
}

}
