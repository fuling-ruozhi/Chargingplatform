#pragma once

#include "model/charger_status_summary.h"
#include "model/charger.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace ncs {

class DatabaseManager;

class ChargerRepository
{
public:
    explicit ChargerRepository(DatabaseManager &database) : database_(database) {}

    bool statusSummary(ChargerStatusSummary *result, QString *error) const;
    bool list(const QString &keyword, int status, QVector<Charger> *result,
              QString *error, qint64 stationId = -1) const;
    bool findById(qint64 id, Charger *result, bool *found, QString *error) const;
    bool stationExists(qint64 stationId, bool *found, QString *error) const;
    bool insert(qint64 stationId, const QString &code, int type, double powerKw,
                Charger *result, QString *error) const;
    bool remove(qint64 id, bool *removed, QString *error) const;
    bool updateStatus(qint64 id, ChargerStatus expected, ChargerStatus next,
                      bool *updated, QString *error) const;
    bool insertBatch(qint64 stationId, const QStringList &codes, int type, double powerKw,
                     QVector<Charger> *result, QString *error) const;

private:
    DatabaseManager &database_;
};

}
