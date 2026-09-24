#pragma once

#include <QMetaType>
#include <QString>

namespace ncs {

enum class ChargingOrderStatus {
    Reserved = 0,
    Charging = 1,
    Completed = 2,
    Cancelled = 3
};

struct ChargingRecord
{
    qint64 id = 0;
    QString orderNo;
    qint64 userId = 0;
    qint64 chargerId = 0;
    QString startTime;
    QString endTime;
    double energy = 0.0;
    double cost = 0.0;
    qint64 durationSeconds = 0;
    ChargingOrderStatus status = ChargingOrderStatus::Charging;
    QString reservedAt;
    QString expireAt;
    qint64 stationId = 0;
    QString stationName;
    QString chargerCode;
    double price = 0.0;
    double powerKw = 0.0;
    int timeScale = 60;
    double initialSoc = 20.0;
    double finalSoc = 20.0;
    double debtAmount = 0.0;
    double balanceAfter = 0.0;
};

}

Q_DECLARE_METATYPE(ncs::ChargingRecord)
Q_DECLARE_METATYPE(ncs::ChargingOrderStatus)
