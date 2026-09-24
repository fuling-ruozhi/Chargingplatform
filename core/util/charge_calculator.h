#pragma once

#include <QDateTime>

namespace ncs {

struct ChargeMetrics
{
    double elapsedRealSeconds = 0.0;
    qint64 elapsedSimSeconds = 0;
    double energyKwh = 0.0;
    double amount = 0.0;
    double powerKw = 0.0;
    double soc = 0.0;
};

class ChargeCalculator
{
public:
    static ChargeMetrics calculate(const QDateTime &start, const QDateTime &current,
                                   double powerKw, double pricePerKwh, int timeScale,
                                   double initialSoc, double batteryCapacityKwh);
};

}
