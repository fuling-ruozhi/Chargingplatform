#include "charge_service.h"

#include "repository/charge_repository.h"
#include "util/logger.h"

namespace ncs {

ServiceResult<QVector<ChargingRecord>> ChargeService::history(
    qint64 userId, int page, int pageSize)
{
    if (userId <= 0 || page < 1 || pageSize < 1 || pageSize > 100) {
        return ServiceResult<QVector<ChargingRecord>>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("分页参数无效"));
    }
    QVector<ChargingRecord> records;
    QString error;
    if (!repository_.listForUser(userId, pageSize, (page - 1) * pageSize,
                                 &records, &error)) {
        Logger::error(QStringLiteral("order-history"), error);
        return ServiceResult<QVector<ChargingRecord>>::fail(
            BusinessErrorCode::DatabaseError,
            QStringLiteral("订单读取失败，请稍后重试"));
    }
    return ServiceResult<QVector<ChargingRecord>>::ok(records);
}

ServiceResult<ChargingRecord> ChargeService::orderDetail(qint64 userId,
                                                         qint64 orderId)
{
    if (userId <= 0 || orderId <= 0) {
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("订单编号无效"));
    }
    ChargingRecord record;
    bool found = false;
    QString error;
    if (!repository_.findForUser(userId, orderId, &record, &found, &error)) {
        Logger::error(QStringLiteral("order-history"), error);
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::DatabaseError,
            QStringLiteral("订单读取失败，请稍后重试"));
    }
    if (!found) {
        return ServiceResult<ChargingRecord>::fail(
            BusinessErrorCode::InvalidOrderOwner,
            QStringLiteral("无权查看该订单"));
    }
    return ServiceResult<ChargingRecord>::ok(record);
}

}
