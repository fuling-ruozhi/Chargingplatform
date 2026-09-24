#pragma once

#include <QMetaType>
#include <QString>

namespace ncs {

struct Review
{
    qint64 id = 0;
    qint64 orderId = 0;
    qint64 userId = 0;
    qint64 stationId = 0;
    qint64 chargerId = 0;
    int environmentScore = 0;
    int queueScore = 0;
    int equipmentScore = 0;
    int parkingScore = 0;
    double overallScore = 0.0;
    QString createdAt;
};

struct StationRatingSummary
{
    qint64 reviewCount = 0;
    double averageScore = 0.0;
    double environmentAverage = 0.0;
    double queueAverage = 0.0;
    double equipmentAverage = 0.0;
    double parkingAverage = 0.0;
};

}

Q_DECLARE_METATYPE(ncs::Review)
Q_DECLARE_METATYPE(ncs::StationRatingSummary)
