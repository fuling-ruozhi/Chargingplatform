#pragma once

#include "model/charger.h"
#include "model/charging_record.h"

#include <QVector>

namespace ncs {

class DatabaseManager;

class ChargeRepository
{
public:
    explicit ChargeRepository(DatabaseManager &database);
    bool userExists(qint64 userId, bool *found, QString *error) const;
    bool userChargeState(qint64 userId, int *status, double *balance,
                         bool *found, QString *error) const;
    bool chargerInfo(qint64 chargerId, ChargerStatus *status, double *price, double *powerKw,
                     qint64 *stationId, QString *stationName, QString *chargerCode,
                     bool *found, QString *error) const;
    bool updateChargerStatus(qint64 chargerId, ChargerStatus expected, ChargerStatus next,
                             bool *updated, QString *error) const;
    bool create(qint64 userId, qint64 chargerId, const QString &startTime,
                int timeScale, double initialSoc,
                ChargingRecord *record, QString *error) const;
    bool createReservation(qint64 userId, qint64 chargerId, const QString &reservedAt,
                           const QString &expireAt, int timeScale, double initialSoc,
                           ChargingRecord *record,
                           QString *error) const;
    bool findActive(qint64 recordId, ChargingRecord *record, bool *found, QString *error) const;
    bool findById(qint64 recordId, ChargingRecord *record, bool *found, QString *error) const;
    bool findActiveForUser(qint64 userId, ChargingRecord *record, bool *found,
                           QString *error) const;
    bool listForUser(qint64 userId, int limit, int offset,
                     QVector<ChargingRecord> *records, QString *error) const;
    bool findForUser(qint64 userId, qint64 orderId, ChargingRecord *record,
                     bool *found, QString *error) const;
    bool startReservation(qint64 recordId, qint64 userId, qint64 chargerId,
                          const QString &startTime, bool *updated, QString *error) const;
    bool cancelReservation(qint64 recordId, qint64 userId, const QString &cancelledAt,
                           bool *updated, QString *error) const;
    bool expiredReservations(const QString &now, QVector<ChargingRecord> *records,
                             QString *error) const;
    bool markReservationExpired(qint64 recordId, const QString &expiredAt,
                                bool *updated, QString *error) const;
    bool finish(qint64 recordId, const QString &endTime, double energy, double cost,
                double finalSoc, double debtAmount, double balanceAfter,
                QString *error) const;
    bool settleCharger(qint64 chargerId, qint64 simulatedMinutes,
                       QString *error) const;
    bool settleUserBalance(qint64 userId, double balanceAfter,
                           QString *error) const;

private:
    DatabaseManager &database_;
};

}
