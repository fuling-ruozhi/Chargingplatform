#pragma once

#include "model/load_prediction.h"

#include <QVector>

namespace ncs {

class DatabaseManager;

class PredictionRepository
{
public:
    explicit PredictionRepository(DatabaseManager &database)
        : database_(database)
    {
    }

    bool list(PredictionList *result, QString *error) const;
    bool latestGeneratedAt(QString *generatedAt, QString *error) const;
    bool hourlyActual(int hours, QVector<HourlyLoad> *result,
                      QString *error) const;

private:
    DatabaseManager &database_;
};

}
