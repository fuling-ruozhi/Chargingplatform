#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

namespace ncs {

struct Station
{
    qint64 id = 0;
    QString name;
    QString address;
    double price = 0.0;
    int totalSlots = 0;
    double longitude = 0.0;
    double latitude = 0.0;
    int idleSlots = 0;
    int chargerCount = 0;
    double distanceKm = 0.0;
    qint64 reviewCount = 0;
    double averageScore = 0.0;
    double queueScore = 0.0;
    double environmentScore = 0.0;
    double equipmentScore = 0.0;
    double parkingScore = 0.0;
    double recommendScore = 0.0;
};

struct StationPreferenceStats
{
    int completedOrders = 0;
    double energyKwh = 0.0;
};

struct StationRecommendation
{
    Station station;
    double score = 0.0;
    double idleRate = 0.0;
    double predictedEnergy = 0.0;
    int preferenceOrders = 0;
    double preferenceEnergy = 0.0;
    QString weatherSummary;
    QStringList reasons;
};

}

Q_DECLARE_METATYPE(ncs::Station)
Q_DECLARE_METATYPE(QVector<ncs::Station>)
Q_DECLARE_METATYPE(ncs::StationRecommendation)
Q_DECLARE_METATYPE(QVector<ncs::StationRecommendation>)
