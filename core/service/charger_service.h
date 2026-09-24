#pragma once

#include "model/charger_status_summary.h"
#include "model/service_result.h"
#include "model/charger.h"

namespace ncs {

class ChargerRepository;

class ChargerService
{
public:
    explicit ChargerService(ChargerRepository &repository) : repository_(repository) {}

    ServiceResult<ChargerStatusSummary> statusSummary() const;
    ServiceResult<QVector<Charger>> list(const QString &keyword, int status) const;
    ServiceResult<QVector<Charger>> list(const QString &keyword, int status,
                                         qint64 stationId) const;
    ServiceResult<Charger> create(qint64 stationId, const QString &code,
                                  int type, double powerKw) const;
    ServiceResult<Charger> remove(qint64 id) const;
    ServiceResult<Charger> markFault(qint64 id) const;
    ServiceResult<Charger> recover(qint64 id) const;
    ServiceResult<Charger> restart(qint64 id) const;
    ServiceResult<QVector<Charger>> batchCreate(qint64 stationId, const QString &prefix,
                                                int count, int type, double powerKw) const;

private:
    ServiceResult<Charger> changeStatus(qint64 id, ChargerStatus expected,
                                        ChargerStatus next) const;
    ChargerRepository &repository_;
};

}
