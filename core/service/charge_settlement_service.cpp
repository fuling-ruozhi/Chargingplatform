#include "charge_service.h"

#include "config/charge_config.h"
#include "database/database_manager.h"
#include "repository/charge_repository.h"
#include "util/charge_calculator.h"
#include "util/date_time_storage.h"
#include "util/money.h"
#include "util/logger.h"

namespace ncs {
namespace {

template<typename T>
ServiceResult<T> databaseFailure(const QString &message)
{
    Logger::error(QStringLiteral("settlement"), message);
    return ServiceResult<T>::fail(BusinessErrorCode::DatabaseError,
                                  QStringLiteral("服务暂时不可用，请稍后重试"));
}

}

ServiceResult<ChargingRecord> ChargeService::stop(qint64 orderId, qint64 userId)
{
    if (orderId <= 0) {
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("订单编号无效"));
    }
    if (!database_.transaction()) return databaseFailure<ChargingRecord>(database_.lastError());
    QString error;
    bool found = false;
    ChargingRecord record;
    if (!repository_.findActive(orderId, &record, &found, &error)) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(error);
    }
    if (!found) {
        ChargingRecord existing;
        bool exists = false;
        if (!repository_.findById(orderId, &existing, &exists, &error)) {
            database_.rollback();
            return databaseFailure<ChargingRecord>(error);
        }
        database_.rollback();
        if (!exists) {
            return ServiceResult<ChargingRecord>::fail(
                BusinessErrorCode::ActiveChargeNotFound,
                QStringLiteral("没有进行中的充电订单"));
        }
        if (userId > 0 && existing.userId != userId) {
            return ServiceResult<ChargingRecord>::fail(
                BusinessErrorCode::InvalidOrderOwner, QStringLiteral("无权操作该订单"));
        }
        if (existing.status == ChargingOrderStatus::Completed) {
            return ServiceResult<ChargingRecord>::ok(existing);
        }
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::InvalidOrderState, QStringLiteral("订单当前不在充电状态"));
    }
    if (userId > 0 && record.userId != userId) {
        database_.rollback();
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::InvalidOrderOwner, QStringLiteral("无权操作该订单"));
    }
    ChargerStatus chargerStatus = ChargerStatus::Fault;
    double currentPrice = 0.0;
    double currentPower = 0.0;
    qint64 stationId = 0;
    QString stationName;
    QString chargerCode;
    bool chargerFound = false;
    if (!repository_.chargerInfo(record.chargerId, &chargerStatus, &currentPrice,
                                 &currentPower, &stationId, &stationName, &chargerCode,
                                 &chargerFound, &error)) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(error);
    }
    if (!chargerFound || chargerStatus != ChargerStatus::Using) {
        database_.rollback();
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::ChargerUnavailable,
            QStringLiteral("充电桩状态与订单不一致"));
    }

    int userStatus = 0;
    double userBalance = 0.0;
    bool userFound = false;
    if (!repository_.userChargeState(record.userId, &userStatus, &userBalance,
                                     &userFound, &error)) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(error);
    }
    Q_UNUSED(userStatus)
    if (!userFound) {
        database_.rollback();
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::UserNotFound, QStringLiteral("用户不存在"));
    }

    const QDateTime end = DateTimeStorage::now();
    const ChargeMetrics metrics = ChargeCalculator::calculate(
        DateTimeStorage::fromText(record.startTime), end, record.powerKw, record.price,
        record.timeScale, record.initialSoc, ChargeConfig::batteryCapacityKwh());
    record.endTime = DateTimeStorage::toText(end);
    record.energy = metrics.energyKwh;
    record.cost = metrics.amount;
    record.durationSeconds = metrics.elapsedSimSeconds;
    record.finalSoc = metrics.soc;
    record.balanceAfter = Money::subtractFloorZero(userBalance, record.cost);
    record.debtAmount = Money::deficit(record.cost, userBalance);
    record.status = ChargingOrderStatus::Completed;
    if (!repository_.finish(record.id, record.endTime, record.energy, record.cost,
                            record.finalSoc, record.debtAmount,
                            record.balanceAfter, &error)) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(error);
    }
    const qint64 simulatedMinutes = record.durationSeconds / 60;
    if (!repository_.settleCharger(record.chargerId, simulatedMinutes, &error)) {
        database_.rollback();
        return databaseFailure<ChargingRecord>(error);
    }
    if (!repository_.settleUserBalance(record.userId, record.balanceAfter, &error)) {
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
