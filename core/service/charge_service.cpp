#include "charge_service.h"

#include "config/charge_config.h"
#include "database/database_manager.h"
#include "repository/charge_repository.h"
#include "util/date_time_storage.h"
#include "util/logger.h"
#include "util/money.h"

#include <QDateTime>
#include <QtGlobal>

namespace ncs {
namespace {

template<typename T>
ServiceResult<T> databaseFailure(const QString &message)
{
    Logger::error(QStringLiteral("charge"), message);
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

ChargeService::ChargeService(DatabaseManager &database, ChargeRepository &repository)
    : database_(database), repository_(repository) {}

ServiceResult<int> ChargeService::validateNewChargeUser(qint64 userId)
{
    int status = 0;
    double balance = 0.0;
    bool found = false;
    QString error;
    if (!repository_.userChargeState(userId, &status, &balance, &found, &error)) {
        return databaseFailure<int>(error);
    }
    if (!found) {
        return ServiceResult<int>::fail(
            BusinessErrorCode::UserNotFound, QStringLiteral("用户不存在"));
    }
    if (status != 1) {
        return ServiceResult<int>::fail(
            BusinessErrorCode::UserFrozen,
            QStringLiteral("账号已冻结，暂不能预约或开始充电"));
    }
    if (Money::lessThan(balance, ChargeConfig::minBalance())) {
        return ServiceResult<int>::fail(
            BusinessErrorCode::InsufficientBalance,
            QStringLiteral("余额不足，请先充值"));
    }
    return ServiceResult<int>::ok(status);
}

ServiceResult<ChargingRecord> ChargeService::startReserved(qint64 userId, qint64 orderId)
{
    if (userId <= 0 || orderId <= 0) {
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("用户或订单编号无效"));
    }
    ChargingRecord order;
    bool found = false;
    QString error;
    if (!repository_.findById(orderId, &order, &found, &error)) {
        return databaseFailure<ChargingRecord>(error);
    }
    if (!found || order.userId != userId) {
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::InvalidOrderOwner,
            QStringLiteral("无权操作该订单"));
    }
    return start(userId, order.chargerId, orderId);
}

ServiceResult<ChargingRecord> ChargeService::start(qint64 userId, qint64 chargerId,
                                                   qint64 recordId)
{
    if (userId <= 0 || chargerId <= 0) {
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("用户或电桩编号无效"));
    }
    if (!database_.transaction()) return databaseFailure<ChargingRecord>(database_.lastError());

    const QString startTime = nowText();
    QString error;
    QVector<ChargingRecord> expired;
    if (!cleanupExpiredInTransaction(startTime, &expired, &error)) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(error);
    }

    const auto userState = validateNewChargeUser(userId);
    if (!userState.success) {
        database_.rollback();
        return ServiceResult<ChargingRecord>::fail(
            userState.code, userState.message);
    }

    if (recordId > 0) {
        if (containsExpired(expired, userId, chargerId, recordId)) {
            if (!database_.commit()) {
                database_.rollback();
                return databaseFailure<ChargingRecord>(database_.lastError());
            }
            return ServiceResult<ChargingRecord>::fail(
                BusinessErrorCode::ReservationExpired,
                QStringLiteral("预约已过期，请重新选择充电桩"));
        }
        ChargingRecord requested;
        bool requestedFound = false;
        if (!repository_.findById(recordId, &requested, &requestedFound, &error)) {
            database_.rollback();
            return databaseFailure<ChargingRecord>(error);
        }
        if (!requestedFound) {
            database_.rollback();
            return ServiceResult<ChargingRecord>::fail(
                BusinessErrorCode::OrderNotFound, QStringLiteral("订单不存在"));
        }
        if (requested.userId != userId) {
            database_.rollback();
            return ServiceResult<ChargingRecord>::fail(
                BusinessErrorCode::InvalidOrderOwner, QStringLiteral("该订单不属于当前用户"));
        }
        if (requested.chargerId != chargerId
            || requested.status != ChargingOrderStatus::Reserved) {
            database_.rollback();
            return ServiceResult<ChargingRecord>::fail(
                BusinessErrorCode::InvalidOrderState,
                QStringLiteral("订单当前不能开始充电"));
        }
        ChargerStatus reservedChargerStatus = ChargerStatus::Fault;
        double reservedPrice = 0.0;
        double reservedPowerKw = 0.0;
        qint64 reservedStationId = 0;
        QString reservedStationName;
        QString reservedChargerCode;
        bool reservedChargerFound = false;
        if (!repository_.chargerInfo(chargerId, &reservedChargerStatus, &reservedPrice,
                                     &reservedPowerKw,
                                     &reservedStationId, &reservedStationName,
                                     &reservedChargerCode, &reservedChargerFound, &error)) {
            database_.rollback();
            return databaseFailure<ChargingRecord>(error);
        }
        if (!reservedChargerFound || reservedChargerStatus != ChargerStatus::Using) {
            database_.rollback();
            return ServiceResult<ChargingRecord>::fail(
                BusinessErrorCode::ChargerUnavailable,
                QStringLiteral("订单与充电桩状态不一致"));
        }
        bool updated = false;
        if (!repository_.startReservation(recordId, userId, chargerId,
                                          startTime, &updated, &error)) {
            database_.rollback();
            return databaseFailure<ChargingRecord>(error);
        }
        if (!updated) {
            database_.rollback();
            return ServiceResult<ChargingRecord>::fail(
                BusinessErrorCode::ReservationExpired,
                QStringLiteral("预约已过期，请重新选择充电桩"));
        }
        if (!repository_.findById(recordId, &requested, &requestedFound, &error)
            || !requestedFound) {
            database_.rollback();
            return databaseFailure<ChargingRecord>(
                error.isEmpty() ? QStringLiteral("订单状态读取失败") : error);
        }
        if (!database_.commit()) {
            database_.rollback();
            return databaseFailure<ChargingRecord>(database_.lastError());
        }
        return ServiceResult<ChargingRecord>::ok(requested);
    }

    ChargingRecord activeOrder;
    bool activeFound = false;
    if (!repository_.findActiveForUser(userId, &activeOrder, &activeFound, &error)) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(error);
    }
    if (activeFound) {
        if (activeOrder.status == ChargingOrderStatus::Reserved
            && activeOrder.chargerId == chargerId) {
            ChargerStatus reservedChargerStatus = ChargerStatus::Fault;
            double reservedPrice = 0.0;
            double reservedPowerKw = 0.0;
            qint64 reservedStationId = 0;
            QString reservedStationName;
            QString reservedChargerCode;
            bool reservedChargerFound = false;
            if (!repository_.chargerInfo(chargerId, &reservedChargerStatus, &reservedPrice,
                                         &reservedPowerKw,
                                         &reservedStationId, &reservedStationName,
                                         &reservedChargerCode, &reservedChargerFound, &error)) {
                database_.rollback();
                return databaseFailure<ChargingRecord>(error);
            }
            if (!reservedChargerFound || reservedChargerStatus != ChargerStatus::Using) {
                database_.rollback();
                return ServiceResult<ChargingRecord>::fail(
                    BusinessErrorCode::ChargerUnavailable,
                    QStringLiteral("订单与充电桩状态不一致"));
            }
            bool updated = false;
            if (!repository_.startReservation(activeOrder.id, userId, chargerId,
                                              startTime, &updated, &error)) {
                database_.rollback();
                return databaseFailure<ChargingRecord>(error);
            }
            if (!updated) {
                database_.rollback();
                return ServiceResult<ChargingRecord>::fail(
                    BusinessErrorCode::ReservationExpired,
                    QStringLiteral("预约已过期，请重新选择充电桩"));
            }
            ChargingRecord started;
            bool found = false;
            if (!repository_.findById(activeOrder.id, &started, &found, &error) || !found) {
                database_.rollback();
                return databaseFailure<ChargingRecord>(
                    error.isEmpty() ? QStringLiteral("订单状态读取失败") : error);
            }
            if (!database_.commit()) {
                database_.rollback();
                return databaseFailure<ChargingRecord>(database_.lastError());
            }
            return ServiceResult<ChargingRecord>::ok(started);
        }
        database_.rollback();
        if (activeOrder.status == ChargingOrderStatus::Charging
            && activeOrder.chargerId == chargerId) {
            return ServiceResult<ChargingRecord>::fail(
                BusinessErrorCode::ChargerUnavailable,
                QStringLiteral("该充电桩已经在充电中"));
        }
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::ActiveOrderExists,
            QStringLiteral("当前已有预约或正在进行的充电订单"));
    }

    if (containsExpired(expired, userId, chargerId)) {
        if (!database_.commit()) {
            database_.rollback();
            return databaseFailure<ChargingRecord>(database_.lastError());
        }
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::ReservationExpired,
            QStringLiteral("预约已过期，请重新选择充电桩"));
    }

    ChargerStatus status = ChargerStatus::Fault;
    double price = 0.0;
    double powerKw = 0.0;
    qint64 stationId = 0;
    QString stationName;
    QString chargerCode;
    bool found = false;
    if (!repository_.chargerInfo(chargerId, &status, &price, &powerKw, &stationId, &stationName,
                                 &chargerCode, &found, &error)) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(error);
    }
    if (!found) {
        database_.rollback();
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::ChargerNotFound, QStringLiteral("电桩不存在"));
    }
    if (status == ChargerStatus::Fault) {
        database_.rollback();
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::ChargerFault, QStringLiteral("该充电桩正在维护，暂不可使用"));
    }
    if (status != ChargerStatus::Idle) {
        database_.rollback();
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::ChargerUnavailable, QStringLiteral("电桩当前不是空闲状态"));
    }
    bool updated = false;
    if (!repository_.updateChargerStatus(chargerId, ChargerStatus::Idle,
                                         ChargerStatus::Using, &updated, &error)) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(error);
    }
    if (!updated) {
        database_.rollback();
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::ChargerUnavailable, QStringLiteral("电桩当前不可用"));
    }
    ChargingRecord record;
    if (!repository_.create(userId, chargerId, startTime,
                            ChargeConfig::timeScale(), ChargeConfig::initialSoc(),
                            &record, &error)) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(error);
    }
    if (!database_.commit()) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(database_.lastError());
    }
    return ServiceResult<ChargingRecord>::ok(record);
}

}
