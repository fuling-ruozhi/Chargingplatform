#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

class QSqlDatabase;

namespace ncs {

struct ChargerSeed
{
    qint64 id = 0;
    qint64 stationId = 0;
    QString stationName;
    QString code;
    double powerKw = 0.0;
    double pricePerKwh = 0.0;
};

bool seedDemoHistory(QSqlDatabase &database,
                     const QList<ChargerSeed> &chargers,
                     QString *error);

}  // namespace ncs
