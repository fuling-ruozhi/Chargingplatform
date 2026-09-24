#include "util/money.h"
#include "util/smart_charging_planner.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace {

bool near(double actual, double expected, double tolerance = 0.0001)
{
    return std::abs(actual - expected) <= tolerance;
}

const ncs::SmartChargingPlan &findPlan(const ncs::SmartChargingResult &result,
                                      ncs::SmartChargingMode mode)
{
    for (const auto &plan : result.plans) {
        if (plan.mode == mode) return plan;
    }
    std::abort();
}

bool limitedBy(const ncs::SmartChargingPlan &plan,
               ncs::SmartChargingLimitingFactor factor)
{
    return plan.limitingFactors.contains(factor);
}

int check(bool condition, int code)
{
    if (!condition) std::cerr << "smart charging assertion failed: " << code << '\n';
    return condition ? 0 : code;
}

}  // namespace

int main()
{
    ncs::SmartChargingInput input;
    input.currentSocPercent = 30.0;
    input.targetSocPercent = 80.0;
    input.batteryCapacityKwh = 60.0;
    input.chargerPowerKw = 120.0;
    input.pricePerKwh = 1.2;
    input.userBalance = 100.0;
    input.reserveBalance = 10.0;
    input.availableMinutes = 60;

    auto result = ncs::SmartChargingPlanner::plan(input);
    if (int error = check(result.success && result.plans.size() == 3, 1)) return error;
    const auto &fast = findPlan(result, ncs::SmartChargingMode::Fastest);
    if (int error = check(near(fast.recommendedTargetSoc, 80.0)
        && near(fast.energyKwh, 30.0) && fast.estimatedMinutes == 15
        && near(fast.estimatedAmount, 36.0) && fast.canReachRequestedTarget, 2)) return error;

    input.userBalance = 22.0;
    result = ncs::SmartChargingPlanner::plan(input);
    const auto &moneyLimited = findPlan(result, ncs::SmartChargingMode::Fastest);
    if (int error = check(near(moneyLimited.energyKwh, 10.0)
        && near(moneyLimited.recommendedTargetSoc, 46.666666, 0.001)
        && limitedBy(moneyLimited, ncs::SmartChargingLimitingFactor::BalanceLimit), 3)) return error;

    input.userBalance = 100.0;
    input.availableMinutes = 3;
    result = ncs::SmartChargingPlanner::plan(input);
    const auto &timeLimited = findPlan(result, ncs::SmartChargingMode::Fastest);
    if (int error = check(near(timeLimited.energyKwh, 6.0)
        && limitedBy(timeLimited, ncs::SmartChargingLimitingFactor::TimeLimit), 4)) return error;

    input.userBalance = 17.2;
    input.availableMinutes = 3;
    result = ncs::SmartChargingPlanner::plan(input);
    const auto &bothLimited = findPlan(result, ncs::SmartChargingMode::Fastest);
    if (int error = check(limitedBy(bothLimited, ncs::SmartChargingLimitingFactor::TimeLimit)
        && limitedBy(bothLimited, ncs::SmartChargingLimitingFactor::BalanceLimit), 5)) return error;

    input.currentSocPercent = 80.0;
    input.targetSocPercent = 80.0;
    input.userBalance = 100.0;
    input.availableMinutes = 60;
    result = ncs::SmartChargingPlanner::plan(input);
    const auto &alreadyThere = findPlan(result, ncs::SmartChargingMode::Balanced);
    if (int error = check(near(alreadyThere.energyKwh, 0.0)
        && alreadyThere.estimatedMinutes == 0
        && near(alreadyThere.estimatedAmount, 0.0), 6)) return error;

    input.currentSocPercent = 81.0;
    if (int error = check(!ncs::SmartChargingPlanner::plan(input).success, 7)) return error;
    input.currentSocPercent = 30.0;
    input.targetSocPercent = 101.0;
    if (int error = check(!ncs::SmartChargingPlanner::plan(input).success, 8)) return error;
    input.targetSocPercent = 80.0;
    input.batteryCapacityKwh = 0.0;
    if (int error = check(!ncs::SmartChargingPlanner::plan(input).success, 9)) return error;
    input.batteryCapacityKwh = 60.0;
    input.chargerPowerKw = 0.0;
    if (int error = check(!ncs::SmartChargingPlanner::plan(input).success, 10)) return error;

    input.chargerPowerKw = 120.0;
    input.pricePerKwh = 0.0;
    input.userBalance = 10.0;
    input.reserveBalance = 10.0;
    result = ncs::SmartChargingPlanner::plan(input);
    if (int error = check(result.success
        && findPlan(result, ncs::SmartChargingMode::Fastest).canReachRequestedTarget, 11)) return error;

    input.pricePerKwh = 1.2;
    input.reserveBalance = 11.0;
    if (int error = check(!ncs::SmartChargingPlanner::plan(input).success, 12)) return error;

    input.reserveBalance = 0.0;
    input.userBalance = 100.0;
    input.targetSocPercent = 90.0;
    result = ncs::SmartChargingPlanner::plan(input);
    const auto &fast90 = findPlan(result, ncs::SmartChargingMode::Fastest);
    const auto &balanced = findPlan(result, ncs::SmartChargingMode::Balanced);
    const auto &economy = findPlan(result, ncs::SmartChargingMode::Economy);
    if (int error = check(fast90.recommendedTargetSoc > balanced.recommendedTargetSoc
        && balanced.recommendedTargetSoc > economy.recommendedTargetSoc, 13)) return error;
    if (int error = check(result.recommendedMode == ncs::SmartChargingMode::Balanced, 14)) return error;

    for (const auto &plan : result.plans) {
        if (int error = check(plan.energyKwh >= 0.0 && plan.estimatedAmount >= 0.0
            && plan.recommendedTargetSoc <= 100.0
            && near(plan.estimatedAmount,
                    ncs::Money::round(plan.energyKwh * input.pricePerKwh)), 15)) return error;
    }

    ncs::SmartChargingInput invalid = input;
    invalid.currentSocPercent = std::numeric_limits<double>::quiet_NaN();
    if (int error = check(!ncs::SmartChargingPlanner::plan(invalid).success, 16)) return error;

    input.userBalance = 12.0;
    input.reserveBalance = 0.0;
    input.availableMinutes = 60;
    result = ncs::SmartChargingPlanner::plan(input);
    if (int error = check(result.recommendedMode == ncs::SmartChargingMode::Economy, 17)) return error;
    input.userBalance = 100.0;
    input.availableMinutes = 2;
    result = ncs::SmartChargingPlanner::plan(input);
    if (int error = check(result.recommendedMode == ncs::SmartChargingMode::Fastest, 18)) return error;

    return 0;
}
