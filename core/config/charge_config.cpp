#include "charge_config.h"

#include <QSettings>

namespace ncs {
namespace {

QSettings settings()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope,
                     QStringLiteral("NCS"), QStringLiteral("charging-platform"));
}

}

double ChargeConfig::minBalance()
{
    const double value = settings().value(QStringLiteral("charge.min_balance"), 5.0)
                             .toDouble();
    return value > 0.0 ? value : 5.0;
}

int ChargeConfig::reservationMinutes()
{
    const int value = settings().value(QStringLiteral("charge.reservation_minutes"), 15)
                          .toInt();
    return value > 0 ? value : 15;
}

int ChargeConfig::timeScale()
{
    const int value = settings().value(QStringLiteral("charge.time_scale"), 60).toInt();
    return value > 0 ? value : 60;
}

double ChargeConfig::initialSoc()
{
    const double value = settings().value(QStringLiteral("charge.initial_soc"), 20.0)
                             .toDouble();
    return value >= 0.0 && value <= 100.0 ? value : 20.0;
}

double ChargeConfig::batteryCapacityKwh()
{
    const double value = settings().value(QStringLiteral("charge.battery_capacity_kwh"),
                                           60.0).toDouble();
    return value > 0.0 ? value : 60.0;
}

double ChargeConfig::defaultDailyKwh()
{
    const double value = settings().value(QStringLiteral("charge.default_daily_kwh"),
                                           8.0).toDouble();
    return value > 0.0 ? value : 8.0;
}

double ChargeConfig::lowSocPercent()
{
    const double value = settings().value(QStringLiteral("charge.low_soc_percent"),
                                           20.0).toDouble();
    return value >= 0.0 && value <= 90.0 ? value : 20.0;
}

}
