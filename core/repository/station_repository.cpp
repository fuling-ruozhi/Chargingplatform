#include "station_repository.h"

#include "database/database_manager.h"
#include "model/charger.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QDateTime>

namespace ncs {

namespace {

Station stationFromQuery(const QSqlQuery &query)
{
    Station station{query.value(0).toLongLong(), query.value(1).toString(),
                    query.value(2).toString(), query.value(3).toDouble(),
                    query.value(4).toInt()};
    station.longitude = query.value(5).toDouble();
    station.latitude = query.value(6).toDouble();
    station.idleSlots = query.value(7).toInt();
    station.chargerCount = query.value(8).toInt();
    station.reviewCount = query.value(9).toLongLong();
    station.averageScore = query.value(10).toDouble();
    station.queueScore = query.value(11).toDouble();
    station.environmentScore = query.value(12).toDouble();
    station.equipmentScore = query.value(13).toDouble();
    station.parkingScore = query.value(14).toDouble();
    return station;
}

const QString stationSelection = QStringLiteral(
    "SELECT s.id,s.name,s.address,s.price,s.total_slots,s.longitude,s.latitude,"
    "COALESCE(SUM(CASE WHEN c.status=:availableStatus THEN 1 ELSE 0 END),0),COUNT(c.id),"
    "COALESCE(rv.review_count,0),COALESCE(rv.average_score,0),COALESCE(rv.queue_score,0),"
    "COALESCE(rv.environment_score,0),COALESCE(rv.equipment_score,0),COALESCE(rv.parking_score,0) "
    "FROM station s LEFT JOIN charger c ON c.station_id=s.id "
    "LEFT JOIN (SELECT station_id,COUNT(*) AS review_count,AVG(overall_score) AS average_score,"
    "AVG(queue_score) AS queue_score,AVG(environment_score) AS environment_score,"
    "AVG(equipment_score) AS equipment_score,AVG(parking_score) AS parking_score "
    "FROM charging_review GROUP BY station_id) rv ON rv.station_id=s.id ");

const QString stationByIdSelection = QStringLiteral(
    "SELECT s.id,s.name,s.address,s.price,s.total_slots,s.longitude,s.latitude,"
    "COALESCE((SELECT COUNT(*) FROM charger c1 WHERE c1.station_id=s.id "
    "AND c1.status=:availableStatus),0),"
    "COALESCE((SELECT COUNT(*) FROM charger c2 WHERE c2.station_id=s.id),0),"
    "COALESCE((SELECT COUNT(*) FROM charging_review r1 WHERE r1.station_id=s.id),0),"
    "COALESCE((SELECT AVG(overall_score) FROM charging_review r2 WHERE r2.station_id=s.id),0),"
    "COALESCE((SELECT AVG(queue_score) FROM charging_review r3 WHERE r3.station_id=s.id),0),"
    "COALESCE((SELECT AVG(environment_score) FROM charging_review r4 WHERE r4.station_id=s.id),0),"
    "COALESCE((SELECT AVG(equipment_score) FROM charging_review r5 WHERE r5.station_id=s.id),0),"
    "COALESCE((SELECT AVG(parking_score) FROM charging_review r6 WHERE r6.station_id=s.id),0) "
    "FROM station s WHERE s.id=:id LIMIT 1");

}

StationRepository::StationRepository(DatabaseManager &database) : database_(database) {}

bool StationRepository::list(QVector<Station> *stations, QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(stationSelection + QStringLiteral(
            "GROUP BY s.id,s.name,s.address,s.price,s.total_slots,s.longitude,s.latitude,"
            "rv.review_count,rv.average_score,rv.queue_score,rv.environment_score,"
            "rv.equipment_score,rv.parking_score ORDER BY s.id"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":availableStatus"),
                    static_cast<int>(ChargerStatus::Idle));
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    stations->clear();
    while (query.next()) stations->append(stationFromQuery(query));
    return true;
}

bool StationRepository::detail(qint64 stationId, StationDetail *detail, bool *found,
                               QString *error) const
{
    QSqlQuery stationQuery(database_.connection());
    if (!stationQuery.prepare(QStringLiteral(
        "SELECT s.id,s.name,s.address,s.price,s.total_slots,s.longitude,s.latitude,"
        "COALESCE(SUM(CASE WHEN c.status=:availableStatus THEN 1 ELSE 0 END),0),COUNT(c.id) "
        "FROM station s LEFT JOIN charger c ON c.station_id=s.id WHERE s.id=:id "
        "GROUP BY s.id,s.name,s.address,s.price,s.total_slots,s.longitude,s.latitude"))) {
        *error = stationQuery.lastError().text();
        return false;
    }
    stationQuery.bindValue(QStringLiteral(":availableStatus"),
                           static_cast<int>(ChargerStatus::Idle));
    stationQuery.bindValue(QStringLiteral(":id"), stationId);
    if (!stationQuery.exec()) {
        *error = stationQuery.lastError().text();
        return false;
    }
    *found = stationQuery.next();
    if (!*found) return true;
    detail->station = {stationQuery.value(0).toLongLong(), stationQuery.value(1).toString(),
                       stationQuery.value(2).toString(), stationQuery.value(3).toDouble(),
                       stationQuery.value(4).toInt()};
    detail->station.longitude = stationQuery.value(5).toDouble();
    detail->station.latitude = stationQuery.value(6).toDouble();
    detail->station.idleSlots = stationQuery.value(7).toInt();
    detail->station.chargerCount = stationQuery.value(8).toInt();
    detail->chargers.clear();
    QSqlQuery chargerQuery(database_.connection());
    if (!chargerQuery.prepare(QStringLiteral(
            "SELECT c.id,c.station_id,c.code,c.status,COALESCE(r.status,-1),"
            "c.type,c.power_kw,c.total_count,c.total_minutes "
            "FROM charger c "
            "LEFT JOIN charging_order r ON r.charger_id=c.id AND r.status IN (0,1) "
            "WHERE c.station_id=:station_id ORDER BY c.id"))) {
        *error = chargerQuery.lastError().text();
        return false;
    }
    chargerQuery.bindValue(QStringLiteral(":station_id"), stationId);
    if (!chargerQuery.exec()) {
        *error = chargerQuery.lastError().text();
        return false;
    }
    while (chargerQuery.next()) {
        Charger charger{chargerQuery.value(0).toLongLong(),
                        chargerQuery.value(1).toLongLong(),
                        chargerQuery.value(2).toString(),
                        static_cast<ChargerStatus>(chargerQuery.value(3).toInt()),
                        chargerQuery.value(4).toInt()};
        charger.type = chargerQuery.value(5).toInt();
        charger.powerKw = chargerQuery.value(6).toDouble();
        charger.totalCount = chargerQuery.value(7).toInt();
        charger.totalMinutes = chargerQuery.value(8).toLongLong();
        detail->chargers.append(charger);
    }
    return true;
}

bool StationRepository::findById(qint64 stationId, Station *station, bool *found,
                                  QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(stationByIdSelection)) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":availableStatus"),
                    static_cast<int>(ChargerStatus::Idle));
    query.bindValue(QStringLiteral(":id"), stationId);
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    *found = query.next();
    if (*found) *station = stationFromQuery(query);
    return true;
}

bool StationRepository::insert(const Station &station, Station *created, QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "INSERT INTO station(name,address,longitude,latitude,price,total_slots,created_at) "
            "VALUES(:name,:address,:longitude,:latitude,:price,:slots,:created_at)"))) {
        *error = query.lastError().text(); return false;
    }
    query.bindValue(QStringLiteral(":name"), station.name);
    query.bindValue(QStringLiteral(":address"), station.address);
    query.bindValue(QStringLiteral(":longitude"), station.longitude);
    query.bindValue(QStringLiteral(":latitude"), station.latitude);
    query.bindValue(QStringLiteral(":price"), station.price);
    query.bindValue(QStringLiteral(":slots"), station.totalSlots);
    query.bindValue(QStringLiteral(":created_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (!query.exec()) { *error = query.lastError().text(); return false; }
    bool found = false;
    return findById(query.lastInsertId().toLongLong(), created, &found, error) && found;
}

bool StationRepository::update(const Station &station, Station *updated, bool *found,
                               QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "UPDATE station SET name=:name,address=:address,longitude=:longitude,"
            "latitude=:latitude,price=:price,total_slots=:slots WHERE id=:id"))) {
        *error = query.lastError().text(); return false;
    }
    query.bindValue(QStringLiteral(":id"), station.id);
    query.bindValue(QStringLiteral(":name"), station.name);
    query.bindValue(QStringLiteral(":address"), station.address);
    query.bindValue(QStringLiteral(":longitude"), station.longitude);
    query.bindValue(QStringLiteral(":latitude"), station.latitude);
    query.bindValue(QStringLiteral(":price"), station.price);
    query.bindValue(QStringLiteral(":slots"), station.totalSlots);
    if (!query.exec()) { *error = query.lastError().text(); return false; }
    if (query.numRowsAffected() == 0) { *found = false; return true; }
    return findById(station.id, updated, found, error);
}

bool StationRepository::remove(qint64 stationId, bool *removed, QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral("DELETE FROM station WHERE id=:id"))) {
        *error = query.lastError().text(); return false;
    }
    query.bindValue(QStringLiteral(":id"), stationId);
    if (!query.exec()) { *error = query.lastError().text(); return false; }
    *removed = query.numRowsAffected() == 1;
    return true;
}

bool StationRepository::chargerCount(qint64 stationId, int *count, QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral("SELECT COUNT(*) FROM charger WHERE station_id=:id"))) {
        *error = query.lastError().text(); return false;
    }
    query.bindValue(QStringLiteral(":id"), stationId);
    if (!query.exec() || !query.next()) { *error = query.lastError().text(); return false; }
    *count = query.value(0).toInt();
    return true;
}

bool StationRepository::userPreferenceStats(
    qint64 userId, QHash<qint64, StationPreferenceStats> *stats, QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT station_id,COUNT(*),COALESCE(SUM(energy),0) "
            "FROM charging_order WHERE user_id=:user_id AND status=2 "
            "GROUP BY station_id"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":user_id"), userId);
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    stats->clear();
    while (query.next()) {
        StationPreferenceStats value;
        value.completedOrders = query.value(1).toInt();
        value.energyKwh = query.value(2).toDouble();
        stats->insert(query.value(0).toLongLong(), value);
    }
    return true;
}

bool StationRepository::latestPredictionEnergy(
    QHash<qint64, double> *energyByStation, QString *error) const
{
    QSqlQuery exists(database_.connection());
    if (!exists.exec(QStringLiteral(
            "SELECT name FROM sqlite_master WHERE type='table' "
            "AND name='load_prediction'"))) {
        *error = exists.lastError().text();
        return false;
    }
    energyByStation->clear();
    if (!exists.next()) return true;

    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT station_id,COALESCE(SUM(predicted_energy),0) "
            "FROM load_prediction "
            "WHERE horizon_hours=24 "
            "AND generated_at=(SELECT MAX(generated_at) FROM load_prediction "
            "WHERE horizon_hours=24) "
            "GROUP BY station_id"))) {
        *error = query.lastError().text();
        return false;
    }
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    while (query.next()) {
        energyByStation->insert(query.value(0).toLongLong(),
                                query.value(1).toDouble());
    }
    return true;
}

}
