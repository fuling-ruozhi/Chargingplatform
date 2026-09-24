#pragma once

#include <QVector>

namespace ncs {

class DatabaseManager;

class FavoriteStationRepository
{
public:
    explicit FavoriteStationRepository(DatabaseManager &database);
    bool listByUser(qint64 userId, QVector<qint64> *stationIds, QString *error) const;
    bool exists(qint64 userId, qint64 stationId, bool *result, QString *error) const;
    bool stationExists(qint64 stationId, bool *result, QString *error) const;
    bool add(qint64 userId, qint64 stationId, bool *added, QString *error) const;
    bool remove(qint64 userId, qint64 stationId, bool *removed, QString *error) const;

private:
    DatabaseManager &database_;
};

}
