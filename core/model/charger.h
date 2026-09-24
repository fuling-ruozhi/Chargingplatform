#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>

namespace ncs {

enum class ChargerStatus { Idle = 0, Using = 1, Fault = 2 };

enum class ChargerType { Slow = 0, Fast = 1 };

bool isValidChargerType(int type);
QString chargerTypeText(int type);

struct Charger
{
    qint64 id = 0;
    qint64 stationId = 0;
    QString code;
    ChargerStatus status = ChargerStatus::Idle;
    // -1 means no active order; 0/1 mirror Reserved/Charging for presentation only.
    int activeOrderStatus = -1;
    int type = 0;
    double powerKw = 0.0;
    int totalCount = 0;
    qint64 totalMinutes = 0;
    QString stationName;
};

QString chargerStatusText(ChargerStatus status);

}

Q_DECLARE_METATYPE(ncs::Charger)
Q_DECLARE_METATYPE(QVector<ncs::Charger>)
