#include "charge_repository.h"

#include "database/database_manager.h"
#include "util/date_time_storage.h"

#include <QSqlError>
#include <QSqlQuery>

namespace ncs {
namespace {

const QString historySelection = QStringLiteral(
    "SELECT o.id,o.order_no,o.user_id,o.charger_id,o.start_time,o.end_time,o.energy,o.amount,"
    "o.status,o.reserved_at,o.expire_at,o.station_id,"
    "COALESCE(NULLIF(o.station_name_snapshot,''),s.name),"
    "COALESCE(NULLIF(o.charger_code_snapshot,''),c.code),"
    "o.price_per_kwh,o.power_kw,o.time_scale,o.initial_soc,o.final_soc,"
    "o.debt_amount,o.balance_after FROM charging_order o "
    "JOIN charger c ON c.id=o.charger_id JOIN station s ON s.id=o.station_id ");

void readHistoryRecord(QSqlQuery &query, ChargingRecord *record)
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

bool ChargeRepository::listForUser(qint64 userId, int limit, int offset,
                                   QVector<ChargingRecord> *records,
                                   QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(historySelection + QStringLiteral(
            "WHERE o.user_id=:user_id ORDER BY o.created_at DESC,o.id DESC "
            "LIMIT :limit OFFSET :offset"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":user_id"), userId);
    query.bindValue(QStringLiteral(":limit"), limit);
    query.bindValue(QStringLiteral(":offset"), offset);
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    records->clear();
    while (query.next()) {
        ChargingRecord record;
        readHistoryRecord(query, &record);
        records->append(record);
    }
    return true;
}

bool ChargeRepository::findForUser(qint64 userId, qint64 orderId,
                                   ChargingRecord *record, bool *found,
                                   QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(historySelection + QStringLiteral(
            "WHERE o.user_id=:user_id AND o.id=:order_id"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":user_id"), userId);
    query.bindValue(QStringLiteral(":order_id"), orderId);
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    *found = query.next();
    if (*found) readHistoryRecord(query, record);
    return true;
}

}
