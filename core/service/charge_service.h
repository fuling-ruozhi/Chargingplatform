#pragma once

#include "model/charging_record.h"
#include "model/service_result.h"

#include <QVector>

namespace ncs {

class ChargeRepository;
class DatabaseManager;

class ChargeService
{
public:
    ChargeService(DatabaseManager &database, ChargeRepository &repository);
    ServiceResult<ChargingRecord> reserve(qint64 userId, qint64 chargerId);
    ServiceResult<ChargingRecord> start(qint64 userId, qint64 chargerId,
                                        qint64 recordId = 0);
    ServiceResult<ChargingRecord> startReserved(qint64 userId, qint64 orderId);
    ServiceResult<ChargingRecord> stop(qint64 orderId, qint64 userId = 0);
    ServiceResult<ChargingRecord> cancel(qint64 userId, qint64 recordId);
    ServiceResult<ChargingRecord> active(qint64 userId);
    ServiceResult<QVector<ChargingRecord>> history(qint64 userId, int page,
                                                    int pageSize);
    ServiceResult<ChargingRecord> orderDetail(qint64 userId, qint64 orderId);
    ServiceResult<int> cleanupExpired();

private:
    ServiceResult<int> validateNewChargeUser(qint64 userId);
    bool cleanupExpiredInTransaction(const QString &now,
                                     QVector<ChargingRecord> *expired,
                                     QString *error);

    DatabaseManager &database_;
    ChargeRepository &repository_;
};

}
