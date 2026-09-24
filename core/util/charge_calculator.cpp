#include "charge_calculator.h"

#include "money.h"

#include <QtGlobal>

namespace ncs {

ChargeMetrics ChargeCalculator::calculate(const QDateTime &start, const QDateTime &current,
                                          double powerKw, double pricePerKwh,
                                          int timeScale, double initialSoc,
                                          double batteryCapacityKwh)
{
    ChargeMetrics result;
    result.powerKw = qMax(0.0, powerKw);
    if (!start.isValid() || !current.isValid() || timeScale <= 0
        || pricePerKwh < 0.0 || batteryCapacityKwh <= 0.0) {
        result.soc = qBound(0.0, initialSoc, 100.0);
        return result;
    }
    result.elapsedRealSeconds = qMax(0.0, start.msecsTo(current) / 1000.0);
    const double simulatedSeconds = result.elapsedRealSeconds * timeScale;
    result.elapsedSimSeconds = qMax<qint64>(0, qRound64(simulatedSeconds));
    result.energyKwh = result.powerKw * simulatedSeconds / 3600.0;
    result.amount = Money::round(result.energyKwh * pricePerKwh);
    result.soc = qBound(0.0,
        initialSoc + result.energyKwh / batteryCapacityKwh * 100.0, 100.0);
    return result;
}

}
