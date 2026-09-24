#include "charge_service.h"

#include "config/charge_config.h"
#include "database/database_manager.h"
#include "repository/charge_repository.h"
#include "util/date_time_storage.h"
#include "util/logger.h"

#include <QDateTime>
#include <QtGlobal>

namespace ncs {
namespace {

template<typename T>
ServiceResult<T> databaseFailure(const QString &message)
{
    Logger::error(QStringLiteral("reservation"), message);
    return ServiceResult<T>::fail(BusinessErrorCode::DatabaseError,
                                  QStringLiteral("服务暂时不可用，请稍后重试"));
}

QString nowText()
{
    return DateTimeStorage::nowText();
}

bool containsExpired(const QVector<ChargingRecord> &records, qint64 userId,
                     qint64 chargerId = 0, qint64 recordId = 0)
{
    for (const ChargingRecord &record : records) {
        if (record.userId != userId) continue;
        if (chargerId > 0 && record.chargerId != chargerId) continue;
        if (recordId > 0 && record.id != recordId) continue;
        return true;
    }
    return false;
}

}

bool ChargeService::cleanupExpiredInTransaction(const QString &now,
                                                QVector<ChargingRecord> *expired,
                                                QString *error)
{
    if (!repository_.expiredReservations(now, expired, error)) return false;
    for (const ChargingRecord &record : *expired) {
        bool cancelled = false;
        if (!repository_.markReservationExpired(record.id, now, &cancelled, error)) return false;
        if (!cancelled) continue;
        bool released = false;
        if (!repository_.updateChargerStatus(record.chargerId, ChargerStatus::Using,
                                             ChargerStatus::Idle, &released, error)) {
            return false;
        }
    }
    return true;
}

ServiceResult<int> ChargeService::cleanupExpired()
{
    if (!database_.transaction()) return databaseFailure<int>(database_.lastError());
    const QString now = nowText();
    QString error;
    QVector<ChargingRecord> expired;
    if (!cleanupExpiredInTransaction(now, &expired, &error)) {
        database_.rollback();
        return databaseFailure<int>(error);
    }
    if (!database_.commit()) {
        database_.rollback();
        return databaseFailure<int>(database_.lastError());
    }
    return ServiceResult<int>::ok(expired.size());
}

ServiceResult<ChargingRecord> ChargeService::reserve(qint64 userId, qint64 chargerId)
{
    if (userId <= 0 || chargerId <= 0) {
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("用户或电桩编号无效"));
    }
    if (!database_.transaction()) return databaseFailure<ChargingRecord>(database_.lastError());

    const QDateTime now = DateTimeStorage::now();
    const QString reservedAt = DateTimeStorage::toText(now);
    const QString expireAt = DateTimeStorage::toText(
        now.addSecs(ChargeConfig::reservationMinutes() * 60));
    QString error;
    QVector<ChargingRecord> expired;
    if (!cleanupExpiredInTransaction(reservedAt, &expired, &error)) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(error);
    }

    const auto userState = validateNewChargeUser(userId);
    if (!userState.success) {
        database_.rollback();
        return ServiceResult<ChargingRecord>::fail(
            userState.code, userState.message);
    }

    ChargingRecord activeOrder;
    bool activeFound = false;
    if (!repository_.findActiveForUser(userId, &activeOrder, &activeFound, &error)) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(error);
    }
    if (activeFound) {
        database_.rollback();
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::ActiveOrderExists,
            QStringLiteral("当前已有预约或正在进行的充电订单"));
    }

    ChargerStatus chargerStatus = ChargerStatus::Fault;
    double price = 0.0;
    double powerKw = 0.0;
    qint64 stationId = 0;
    QString stationName;
    QString chargerCode;
    bool chargerFound = false;
    if (!repository_.chargerInfo(chargerId, &chargerStatus, &price, &powerKw, &stationId,
                                 &stationName, &chargerCode, &chargerFound, &error)) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(error);
    }
    if (!chargerFound) {
        database_.rollback();
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::ChargerNotFound, QStringLiteral("充电桩不存在"));
    }
    if (chargerStatus == ChargerStatus::Fault) {
        database_.rollback();
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::ChargerFault, QStringLiteral("该充电桩正在维护，暂不可预约"));
    }
    if (chargerStatus != ChargerStatus::Idle) {
        database_.rollback();
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::ConcurrentReservationConflict,
            QStringLiteral("该充电桩刚刚被其他用户占用，请重新选择"));
    }

    bool occupied = false;
    if (!repository_.updateChargerStatus(chargerId, ChargerStatus::Idle,
                                         ChargerStatus::Using, &occupied, &error)) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(error);
    }
    if (!occupied) {
        database_.rollback();
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::ConcurrentReservationConflict,
            QStringLiteral("该充电桩刚刚被其他用户占用，请重新选择"));
    }

    ChargingRecord record;
    if (!repository_.createReservation(userId, chargerId, reservedAt, expireAt,
                                       ChargeConfig::timeScale(), ChargeConfig::initialSoc(),
                                       &record, &error)) {
        database_.rollback();
        const BusinessErrorCode code = error.contains(QStringLiteral("UNIQUE"), Qt::CaseInsensitive)
            ? BusinessErrorCode::ActiveOrderExists : BusinessErrorCode::DatabaseError;
        if (code == BusinessErrorCode::DatabaseError) {
            return databaseFailure<ChargingRecord>(error);
        }
        return ServiceResult<ChargingRecord>::fail(
            code, QStringLiteral("当前已有预约或正在进行的充电订单"));
    }
    if (!database_.commit()) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(database_.lastError());
    }
    return ServiceResult<ChargingRecord>::ok(record);
}

ServiceResult<ChargingRecord> ChargeService::cancel(qint64 userId, qint64 recordId)
{
    if (userId <= 0 || recordId <= 0) {
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("用户或订单编号无效"));
    }
    if (!database_.transaction()) return databaseFailure<ChargingRecord>(database_.lastError());

    const QString cancelledAt = nowText();
    QString error;
    QVector<ChargingRecord> expired;
    if (!cleanupExpiredInTransaction(cancelledAt, &expired, &error)) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(error);
    }
    if (containsExpired(expired, userId, 0, recordId)) {
        if (!database_.commit()) {
            database_.rollback();
            return databaseFailure<ChargingRecord>(database_.lastError());
        }
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::ReservationExpired,
            QStringLiteral("预约已过期，充电桩已自动释放"));
    }

    ChargingRecord record;
    bool found = false;
    if (!repository_.findById(recordId, &record, &found, &error)) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(error);
    }
    if (!found) {
        database_.rollback();
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::OrderNotFound, QStringLiteral("订单不存在"));
    }
    if (record.userId != userId) {
        database_.rollback();
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::InvalidOrderOwner, QStringLiteral("该订单不属于当前用户"));
    }
    if (record.status != ChargingOrderStatus::Reserved) {
        database_.rollback();
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::InvalidOrderState,
            QStringLiteral("只有已预约订单可以取消"));
    }

    bool cancelled = false;
    if (!repository_.cancelReservation(recordId, userId, cancelledAt,
                                       &cancelled, &error)) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(error);
    }
    if (!cancelled) {
        database_.rollback();
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::InvalidOrderState, QStringLiteral("订单状态已经发生变化"));
    }
    bool released = false;
    if (!repository_.updateChargerStatus(record.chargerId, ChargerStatus::Using,
                                         ChargerStatus::Idle, &released, &error)) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(error);
    }
    if (!released) {
        database_.rollback();
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::ChargerUnavailable,
            QStringLiteral("订单与充电桩状态不一致"));
    }
    if (!repository_.findById(recordId, &record, &found, &error) || !found) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(
            error.isEmpty() ? QStringLiteral("订单状态读取失败") : error);
    }
    if (!database_.commit()) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(database_.lastError());
    }
    return ServiceResult<ChargingRecord>::ok(record);
}

ServiceResult<ChargingRecord> ChargeService::active(qint64 userId)
{
    if (userId <= 0) {
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("用户编号无效"));
    }
    if (!database_.transaction()) return databaseFailure<ChargingRecord>(database_.lastError());
    const QString now = nowText();
    QString error;
    QVector<ChargingRecord> expired;
    if (!cleanupExpiredInTransaction(now, &expired, &error)) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(error);
    }
    bool userFound = false;
    if (!repository_.userExists(userId, &userFound, &error)) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(error);
    }
    if (!userFound) {
        database_.rollback();
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::UserNotFound, QStringLiteral("用户不存在"));
    }
    ChargingRecord record;
    bool found = false;
    if (!repository_.findActiveForUser(userId, &record, &found, &error)) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(error);
    }
    if (!database_.commit()) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(database_.lastError());
    }
    return ServiceResult<ChargingRecord>::ok(found ? record : ChargingRecord());
}


}
