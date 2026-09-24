#pragma once

#include <QString>
#include <QVector>

namespace ncs {

enum class SmartChargingMode { Fastest, Balanced, Economy };

enum class SmartChargingLimitingFactor {
    None,
    TargetReached,
    TimeLimit,
    BalanceLimit,
    BatteryFull
};

struct SmartChargingInput
{
    double currentSocPercent = 30.0;
    double targetSocPercent = 80.0;
    double batteryCapacityKwh = 60.0;
    double chargerPowerKw = 0.0;
    double pricePerKwh = 0.0;
    double userBalance = 0.0;
    double reserveBalance = 10.0;
    int availableMinutes = 60;
};

struct SmartChargingPlan
{
    SmartChargingMode mode = SmartChargingMode::Balanced;
    double requestedTargetSoc = 0.0;
    double recommendedTargetSoc = 0.0;
    double energyKwh = 0.0;
    int estimatedMinutes = 0;
    double estimatedAmount = 0.0;
    double balanceAfter = 0.0;
    bool canReachRequestedTarget = false;
    bool canFinishBeforeDeparture = false;
    QVector<SmartChargingLimitingFactor> limitingFactors;
    QString title;
    QString summary;
};

struct SmartChargingResult
{
    bool success = false;
    QString error;
    QVector<SmartChargingPlan> plans;
    SmartChargingMode recommendedMode = SmartChargingMode::Balanced;
};

class SmartChargingPlanner
{
public:
    static SmartChargingResult plan(const SmartChargingInput &input);
    static QString modeText(SmartChargingMode mode);
    static QString limitingFactorText(SmartChargingLimitingFactor factor);
};

}
