#include "charge_repository.h"

#include "database/database_manager.h"
#include "util/date_time_storage.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QDateTime>
#include <QUuid>

namespace ncs {

namespace {

const QString recordSelection = QStringLiteral(
    "SELECT o.id,o.order_no,o.user_id,o.charger_id,o.start_time,o.end_time,o.energy,o.amount,"
    "o.status,o.reserved_at,o.expire_at,o.station_id,"
    "COALESCE(NULLIF(o.station_name_snapshot,''),s.name),"
    "COALESCE(NULLIF(o.charger_code_snapshot,''),c.code),"
    "o.price_per_kwh,o.power_kw,o.time_scale,o.initial_soc,o.final_soc,"
    "o.debt_amount,o.balance_after FROM charging_order o "
    "JOIN charger c ON c.id=o.charger_id "
    "JOIN station s ON s.id=o.station_id ");

QString nextOrderNo()
{
    return QStringLiteral("NCS%1%2")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMddhhmmsszzz")),
             QUuid::createUuid().toString(QUuid::WithoutBraces).left(6));
}

void readRecord(QSqlQuery &query, ChargingRecord *record)
{
    record->id = query.value(0).toLongLong();
    record->orderNo = query.value(1).toString();
    record->userId = query.value(2).toLongLong();
    record->chargerId = query.value(3).toLongLong();
    record->startTime = query.value(4).toString();
    record->endTime = query.value(5).toString();
    record->energy = query.value(6).toDouble();
    record->cost = query.value(7).toDouble();
    record->status = static_cast<ChargingOrderStatus>(query.value(8).toInt());
    record->reservedAt = query.value(9).toString();
    record->expireAt = query.value(10).toString();
    record->stationId = query.value(11).toLongLong();
    record->stationName = query.value(12).toString();
    record->chargerCode = query.value(13).toString();
    record->price = query.value(14).toDouble();
    record->powerKw = query.value(15).toDouble();
    record->timeScale = query.value(16).toInt();
    record->initialSoc = query.value(17).toDouble();
    record->finalSoc = query.value(18).toDouble();
    record->debtAmount = query.value(19).toDouble();
    record->balanceAfter = query.value(20).toDouble();
    const QDateTime start = DateTimeStorage::fromText(record->startTime);
    const QDateTime end = DateTimeStorage::fromText(record->endTime);
    if (start.isValid() && end.isValid()) {
        record->durationSeconds = qMax<qint64>(0, qRound64(
            start.msecsTo(end) / 1000.0 * record->timeScale));
    }
}

}

ChargeRepository::ChargeRepository(DatabaseManager &database) : database_(database) {}

bool ChargeRepository::userExists(qint64 userId, bool *found, QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral("SELECT 1 FROM user WHERE id=:id"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":id"), userId);
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    *found = query.next();
    return true;
}

bool ChargeRepository::userChargeState(qint64 userId, int *status, double *balance,
                                       bool *found, QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT status,balance FROM user WHERE id=:id"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":id"), userId);
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    *found = query.next();
    if (*found) {
        *status = query.value(0).toInt();
        *balance = query.value(1).toDouble();
    }
    return true;
}

bool ChargeRepository::chargerInfo(qint64 chargerId, ChargerStatus *status, double *price,
                                   double *powerKw,
                                   qint64 *stationId, QString *stationName,
                                   QString *chargerCode, bool *found, QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT c.status,s.price,c.power_kw,c.station_id,s.name,c.code "
            "FROM charger c JOIN station s ON s.id=c.station_id "
            "WHERE c.id=:id"))) { *error = query.lastError().text(); return false; }
    query.bindValue(QStringLiteral(":id"), chargerId);
    if (!query.exec()) { *error = query.lastError().text(); return false; }
    *found = query.next();
    if (*found) {
        *status = static_cast<ChargerStatus>(query.value(0).toInt());
        *price = query.value(1).toDouble();
        *powerKw = query.value(2).toDouble();
        *stationId = query.value(3).toLongLong();
        *stationName = query.value(4).toString();
        *chargerCode = query.value(5).toString();
    }
    return true;
}

bool ChargeRepository::updateChargerStatus(qint64 chargerId, ChargerStatus expected,
                                           ChargerStatus next, bool *updated,
                                           QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "UPDATE charger SET status=:next WHERE id=:id AND status=:expected"))) {
        *error = query.lastError().text(); return false;
    }
    query.bindValue(QStringLiteral(":next"), static_cast<int>(next));
    query.bindValue(QStringLiteral(":id"), chargerId);
    query.bindValue(QStringLiteral(":expected"), static_cast<int>(expected));
    if (!query.exec()) { *error = query.lastError().text(); return false; }
    *updated = query.numRowsAffected() == 1;
    return true;
}

bool ChargeRepository::create(qint64 userId, qint64 chargerId, const QString &startTime,
                              int timeScale, double initialSoc,
                              ChargingRecord *record, QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "INSERT INTO charging_order(order_no,user_id,charger_id,station_id,status,"
            "reserved_at,expire_at,start_time,end_time,energy,amount,price_per_kwh,power_kw,"
            "time_scale,initial_soc,final_soc,station_name_snapshot,"
            "charger_code_snapshot,created_at,updated_at) "
            "SELECT :order_no,:user_id,c.id,c.station_id,1,:start_time,'',:start_time,'',"
            "0,0,s.price,c.power_kw,:time_scale,:initial_soc,:initial_soc,"
            "s.name,c.code,:start_time,:start_time "
            "FROM charger c JOIN station s ON s.id=c.station_id WHERE c.id=:charger_id"))) {
        *error = query.lastError().text(); return false;
    }
    query.bindValue(QStringLiteral(":order_no"), nextOrderNo());
    query.bindValue(QStringLiteral(":user_id"), userId);
    query.bindValue(QStringLiteral(":charger_id"), chargerId);
    query.bindValue(QStringLiteral(":start_time"), startTime);
    query.bindValue(QStringLiteral(":time_scale"), timeScale);
    query.bindValue(QStringLiteral(":initial_soc"), initialSoc);
    if (!query.exec()) { *error = query.lastError().text(); return false; }
    bool found = false;
    return findById(query.lastInsertId().toLongLong(), record, &found, error) && found;
}

bool ChargeRepository::createReservation(qint64 userId, qint64 chargerId,
                                         const QString &reservedAt, const QString &expireAt,
                                         int timeScale, double initialSoc,
                                         ChargingRecord *record, QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "INSERT INTO charging_order(order_no,user_id,charger_id,station_id,status,"
            "reserved_at,expire_at,start_time,end_time,energy,amount,price_per_kwh,power_kw,"
            "time_scale,initial_soc,final_soc,station_name_snapshot,"
            "charger_code_snapshot,created_at,updated_at) "
            "SELECT :order_no,:user_id,c.id,c.station_id,0,:reserved_at,:expire_at,'','',"
            "0,0,s.price,c.power_kw,:time_scale,:initial_soc,:initial_soc,"
            "s.name,c.code,:reserved_at,:reserved_at "
            "FROM charger c JOIN station s ON s.id=c.station_id WHERE c.id=:charger_id"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":order_no"), nextOrderNo());
    query.bindValue(QStringLiteral(":user_id"), userId);
    query.bindValue(QStringLiteral(":charger_id"), chargerId);
    query.bindValue(QStringLiteral(":reserved_at"), reservedAt);
    query.bindValue(QStringLiteral(":expire_at"), expireAt);
    query.bindValue(QStringLiteral(":time_scale"), timeScale);
    query.bindValue(QStringLiteral(":initial_soc"), initialSoc);
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    bool found = false;
    return findById(query.lastInsertId().toLongLong(), record, &found, error) && found;
}

bool ChargeRepository::findActive(qint64 recordId, ChargingRecord *record,
                                  bool *found, QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(recordSelection + QStringLiteral(
            "WHERE o.id=:id AND o.status=1 AND o.end_time=''"))) {
        *error = query.lastError().text(); return false;
    }
    query.bindValue(QStringLiteral(":id"), recordId);
    if (!query.exec()) { *error = query.lastError().text(); return false; }
    *found = query.next();
    if (*found) readRecord(query, record);
    return true;
}

bool ChargeRepository::findById(qint64 recordId, ChargingRecord *record,
                                bool *found, QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(recordSelection + QStringLiteral("WHERE o.id=:id"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":id"), recordId);
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    *found = query.next();
    if (*found) readRecord(query, record);
    return true;
}

bool ChargeRepository::findActiveForUser(qint64 userId, ChargingRecord *record,
                                         bool *found, QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(recordSelection + QStringLiteral(
            "WHERE o.user_id=:user_id AND o.status IN (0,1) ORDER BY o.id DESC LIMIT 1"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":user_id"), userId);
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    *found = query.next();
    if (*found) readRecord(query, record);
    return true;
}

bool ChargeRepository::startReservation(qint64 recordId, qint64 userId, qint64 chargerId,
                                        const QString &startTime, bool *updated,
                                        QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "UPDATE charging_order SET status=1,start_time=:start_time,updated_at=:start_time "
            "WHERE id=:id AND user_id=:user_id AND charger_id=:charger_id AND status=0 "
            "AND datetime(expire_at)>datetime(:start_time)"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":start_time"), startTime);
    query.bindValue(QStringLiteral(":id"), recordId);
    query.bindValue(QStringLiteral(":user_id"), userId);
    query.bindValue(QStringLiteral(":charger_id"), chargerId);
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    *updated = query.numRowsAffected() == 1;
    return true;
}

bool ChargeRepository::cancelReservation(qint64 recordId, qint64 userId,
                                         const QString &cancelledAt, bool *updated,
                                         QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "UPDATE charging_order SET status=3,end_time=:cancelled_at,updated_at=:cancelled_at "
            "WHERE id=:id AND user_id=:user_id AND status=0"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":cancelled_at"), cancelledAt);
    query.bindValue(QStringLiteral(":id"), recordId);
    query.bindValue(QStringLiteral(":user_id"), userId);
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    *updated = query.numRowsAffected() == 1;
    return true;
}

bool ChargeRepository::expiredReservations(const QString &now,
                                           QVector<ChargingRecord> *records,
                                           QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(recordSelection + QStringLiteral(
            "WHERE o.status=0 AND o.expire_at<>'' "
            "AND datetime(o.expire_at)<=datetime(:now) ORDER BY o.id"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":now"), now);
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    records->clear();
    while (query.next()) {
        ChargingRecord record;
        readRecord(query, &record);
        records->append(record);
    }
    return true;
}

bool ChargeRepository::markReservationExpired(qint64 recordId, const QString &expiredAt,
                                              bool *updated, QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "UPDATE charging_order SET status=3,end_time=:expired_at,updated_at=:expired_at "
            "WHERE id=:id AND status=0 "
            "AND datetime(expire_at)<=datetime(:expired_at)"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":expired_at"), expiredAt);
    query.bindValue(QStringLiteral(":id"), recordId);
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    *updated = query.numRowsAffected() == 1;
    return true;
}

}
