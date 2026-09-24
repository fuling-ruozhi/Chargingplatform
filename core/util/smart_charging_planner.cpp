#include "smart_charging_planner.h"

#include "money.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <limits>

namespace ncs {
namespace {

constexpr double Epsilon = 0.000001;

bool finite(double value)
{
    return std::isfinite(value);
}

bool contains(const QVector<SmartChargingLimitingFactor> &factors,
              SmartChargingLimitingFactor factor)
{
    return factors.contains(factor);
}

double energyForTarget(const SmartChargingInput &input, double targetSoc)
{
    return input.batteryCapacityKwh
        * qMax(0.0, targetSoc - input.currentSocPercent) / 100.0;
}

SmartChargingPlan makePlan(const SmartChargingInput &input, SmartChargingMode mode,
                           double modeTarget, double energyByMoney,
                           double energyByTime)
{
    SmartChargingPlan plan;
    plan.mode = mode;
    plan.title = SmartChargingPlanner::modeText(mode);
    plan.requestedTargetSoc = input.targetSocPercent;

    modeTarget = qBound(input.currentSocPercent, modeTarget, 100.0);
    const double requiredEnergy = energyForTarget(input, modeTarget);
    const double batteryRoom = energyForTarget(input, 100.0);
    plan.energyKwh = qMax(0.0, std::min({requiredEnergy, energyByMoney,
                                        energyByTime, batteryRoom}));
    plan.recommendedTargetSoc = qBound(
        input.currentSocPercent,
        input.currentSocPercent
            + plan.energyKwh / input.batteryCapacityKwh * 100.0,
        100.0);
    plan.estimatedMinutes = plan.energyKwh <= Epsilon ? 0
        : static_cast<int>(std::ceil(
              plan.energyKwh / input.chargerPowerKw * 60.0 - Epsilon));
    plan.estimatedAmount = Money::round(plan.energyKwh * input.pricePerKwh);
    plan.balanceAfter = Money::round(qMax(0.0,
        input.userBalance - plan.estimatedAmount));
    plan.canReachRequestedTarget =
        plan.recommendedTargetSoc + Epsilon >= input.targetSocPercent;
    plan.canFinishBeforeDeparture =
        plan.estimatedMinutes <= input.availableMinutes;

    if (requiredEnergy <= Epsilon
        || plan.energyKwh + Epsilon >= requiredEnergy) {
        plan.limitingFactors.append(SmartChargingLimitingFactor::TargetReached);
        if (modeTarget >= 100.0 - Epsilon) {
            plan.limitingFactors.append(SmartChargingLimitingFactor::BatteryFull);
        }
    } else {
        if (energyByMoney + Epsilon < requiredEnergy
            && energyByMoney <= plan.energyKwh + Epsilon) {
            plan.limitingFactors.append(SmartChargingLimitingFactor::BalanceLimit);
        }
        if (energyByTime + Epsilon < requiredEnergy
            && energyByTime <= plan.energyKwh + Epsilon) {
            plan.limitingFactors.append(SmartChargingLimitingFactor::TimeLimit);
        }
        if (batteryRoom + Epsilon < requiredEnergy
            && batteryRoom <= plan.energyKwh + Epsilon) {
            plan.limitingFactors.append(SmartChargingLimitingFactor::BatteryFull);
        }
        if (plan.limitingFactors.isEmpty()) {
            plan.limitingFactors.append(SmartChargingLimitingFactor::None);
        }
    }

    const QString figures = QStringLiteral(
        "建议从 %1% 充至 %2%，补充约 %3 kWh，预计 %4 分钟、费用 ¥%5。")
        .arg(input.currentSocPercent, 0, 'f', 1)
        .arg(plan.recommendedTargetSoc, 0, 'f', 1)
        .arg(plan.energyKwh, 0, 'f', 1)
        .arg(plan.estimatedMinutes)
        .arg(plan.estimatedAmount, 0, 'f', 2);
    QString reason;
    if (contains(plan.limitingFactors, SmartChargingLimitingFactor::BalanceLimit)) {
        reason += QStringLiteral("受账户可用余额限制，已下调建议目标。 ");
    }
    if (contains(plan.limitingFactors, SmartChargingLimitingFactor::TimeLimit)) {
        reason += QStringLiteral("受预计离开时间限制，已按可用时间调整。 ");
    }
    if (mode == SmartChargingMode::Economy && requiredEnergy <= Epsilon) {
        reason += QStringLiteral("当前电量已达到省钱模式参考目标。 ");
    } else if (mode == SmartChargingMode::Balanced) {
        reason += QStringLiteral("该方案采用系统默认均衡参考目标，兼顾电量、时间与费用。 ");
    } else if (mode == SmartChargingMode::Fastest) {
        reason += QStringLiteral("该方案在当前功率与约束内尽可能接近你的目标。 ");
    } else {
        reason += QStringLiteral("该方案避免超过 60% 省钱参考目标的不必要补能。 ");
    }
    plan.summary = figures + QStringLiteral(" ") + reason.trimmed();
    return plan;
}

QString validate(const SmartChargingInput &input)
{
    if (!finite(input.currentSocPercent) || input.currentSocPercent < 0.0
        || input.currentSocPercent > 100.0) {
        return QStringLiteral("当前电量必须在 0% 到 100% 之间");
    }
    if (!finite(input.targetSocPercent)
        || input.targetSocPercent < input.currentSocPercent
        || input.targetSocPercent > 100.0) {
        return QStringLiteral("目标电量必须不低于当前电量且不超过 100%");
    }
    if (!finite(input.batteryCapacityKwh) || input.batteryCapacityKwh <= 0.0) {
        return QStringLiteral("电池容量必须大于 0");
    }
    if (!finite(input.chargerPowerKw) || input.chargerPowerKw <= 0.0) {
        return QStringLiteral("充电桩功率必须大于 0");
    }
    if (!finite(input.pricePerKwh) || input.pricePerKwh < 0.0) {
        return QStringLiteral("电价不能为负数");
    }
    if (!finite(input.userBalance) || input.userBalance < 0.0) {
        return QStringLiteral("账户余额不能为负数");
    }
    if (!finite(input.reserveBalance) || input.reserveBalance < 0.0
        || input.reserveBalance > input.userBalance) {
        return QStringLiteral("保留余额必须在 0 与当前余额之间");
    }
    if (input.availableMinutes < 0) {
        return QStringLiteral("预计离开时间不能为负数");
    }
    return QString();
}

}  // namespace

SmartChargingResult SmartChargingPlanner::plan(const SmartChargingInput &input)
{
    SmartChargingResult result;
    result.error = validate(input);
    if (!result.error.isEmpty()) return result;

    const double availableMoney = qMax(0.0,
        input.userBalance - input.reserveBalance);
    const double energyByMoney = input.pricePerKwh <= Epsilon
        ? std::numeric_limits<double>::infinity()
        : availableMoney / input.pricePerKwh;
    const double energyByTime = input.chargerPowerKw
        * input.availableMinutes / 60.0;
    const double economyTarget = qMin(input.targetSocPercent,
        qMax(input.currentSocPercent, 60.0));
    const double balancedTarget = qMax(input.currentSocPercent,
        qMin(input.targetSocPercent, 80.0));

    result.plans.append(makePlan(input, SmartChargingMode::Fastest,
                                 input.targetSocPercent, energyByMoney,
                                 energyByTime));
    result.plans.append(makePlan(input, SmartChargingMode::Balanced,
                                 balancedTarget, energyByMoney, energyByTime));
    result.plans.append(makePlan(input, SmartChargingMode::Economy,
                                 economyTarget, energyByMoney, energyByTime));

    const double economyRequired = energyForTarget(input, economyTarget);
    const double balancedRequired = energyForTarget(input, balancedTarget);
    if (energyByMoney + Epsilon < economyRequired) {
        result.recommendedMode = SmartChargingMode::Economy;
    } else if (energyByTime + Epsilon < balancedRequired) {
        result.recommendedMode = SmartChargingMode::Fastest;
    } else {
        result.recommendedMode = SmartChargingMode::Balanced;
    }
    result.success = true;
    return result;
}

QString SmartChargingPlanner::modeText(SmartChargingMode mode)
{
    switch (mode) {
    case SmartChargingMode::Fastest: return QStringLiteral("最快方案");
    case SmartChargingMode::Balanced: return QStringLiteral("均衡方案");
    case SmartChargingMode::Economy: return QStringLiteral("省钱方案");
    }
    return QStringLiteral("智慧方案");
}

QString SmartChargingPlanner::limitingFactorText(
    SmartChargingLimitingFactor factor)
{
    switch (factor) {
    case SmartChargingLimitingFactor::TargetReached: return QStringLiteral("目标可达");
    case SmartChargingLimitingFactor::TimeLimit: return QStringLiteral("时间约束");
    case SmartChargingLimitingFactor::BalanceLimit: return QStringLiteral("余额约束");
    case SmartChargingLimitingFactor::BatteryFull: return QStringLiteral("电池上限");
    case SmartChargingLimitingFactor::None: return QStringLiteral("无额外约束");
    }
    return QStringLiteral("未知约束");
}

}
