#pragma once

#include "model/station.h"
#include "model/station_detail.h"

#include <QHash>

namespace ncs {

class DatabaseManager;

class StationRepository
{
public:
    explicit StationRepository(DatabaseManager &database);
    bool list(QVector<Station> *stations, QString *error) const;
    bool findById(qint64 stationId, Station *station, bool *found, QString *error) const;
    bool insert(const Station &station, Station *created, QString *error) const;
    bool update(const Station &station, Station *updated, bool *found, QString *error) const;
    bool remove(qint64 stationId, bool *removed, QString *error) const;
    bool chargerCount(qint64 stationId, int *count, QString *error) const;
    bool detail(qint64 stationId, StationDetail *detail, bool *found, QString *error) const;
    bool userPreferenceStats(qint64 userId,
                             QHash<qint64, StationPreferenceStats> *stats,
                             QString *error) const;
    bool latestPredictionEnergy(QHash<qint64, double> *energyByStation,
                                QString *error) const;

private:
    DatabaseManager &database_;
};

}
