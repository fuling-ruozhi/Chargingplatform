// EXT-SC-03 充电间隔分析与续航预测：纯计算逻辑测试
#include "config/charge_config.h"
#include "service/charge_forecast_service.h"

#include <QDateTime>
#include <cmath>

namespace {

constexpr double kCapacity = 60.0;
constexpr double kDefaultDaily = 8.0;
constexpr double kLowSoc = 20.0;

ncs::ChargeForecastService::Input baseInput()
{
    ncs::ChargeForecastService::Input input;
    input.now = QDateTime(QDate(2026, 9, 8), QTime(10, 0));
    input.capacityKwh = kCapacity;
    input.defaultDailyKwh = kDefaultDaily;
    input.lowSocPercent = kLowSoc;
    return input;
}

int expectNear(double actual, double expected, double epsilon)
{
    return std::abs(actual - expected) <= epsilon ? 0 : 1;
}

}

int main()
{
    // 1) 充电中：预测暂停
    {
        auto input = baseInput();
        input.hasActiveOrder = true;
        const auto forecast = ncs::ChargeForecastService::compute(input);
        if (!forecast.charging || forecast.method != QStringLiteral("充电中"))
            return 1;
    }

    // 2) 冷启动（0 订单）：默认规则，SoC 取充电初始值，剩余电量为 0 天
    {
        auto input = baseInput();
        input.completedCount = 0;
        input.lastFinalSoc = ncs::ChargeConfig::initialSoc();
        const auto forecast = ncs::ChargeForecastService::compute(input);
        if (!forecast.coldStart
            || forecast.method != QStringLiteral("默认规则")
            || expectNear(forecast.dailyKwh, kDefaultDaily, 1e-6) != 0
            || expectNear(forecast.currentSoc, 20.0, 1e-6) != 0
            || expectNear(forecast.remainingDays, 0.0, 1e-6) != 0)
            return 2;
    }

    // 3) 正常历史：习惯法短于电量法时取习惯法
    //    日均 = (10 + 70/7) / 2 = 10；SoC = 80 − 10/60×100 ≈ 63.33
    //    电量法 = (63.33−20)/100×60/10 ≈ 2.6；习惯 = median(2,2,2,2) − 1 = 1
    {
        auto input = baseInput();
        input.completedCount = 5;
        input.lastFinalSoc = 80.0;
        input.lastEndTime = input.now.addDays(-1);
        input.intervalDays = {2.0, 2.0, 2.0, 2.0};
        input.intervalDailyKwh = {10.0, 10.0, 10.0, 10.0};
        input.last7DayKwh = 70.0;
        const auto forecast = ncs::ChargeForecastService::compute(input);
        if (forecast.coldStart
            || expectNear(forecast.dailyKwh, 10.0, 1e-6) != 0
            || expectNear(forecast.currentSoc, 63.3333, 0.01) != 0
            || expectNear(forecast.remainingDays, 1.0, 1e-6) != 0
            || forecast.method != QStringLiteral("习惯法")
            || forecast.suggestedDate != QStringLiteral("2026-09-09"))
            return 3;
    }

    // 4) 电量法更短时取电量法
    //    日均 10，SoC = 60 − 0 = 60，电量法 = 40/100×60/10 = 2.4 天
    {
        auto input = baseInput();
        input.completedCount = 3;
        input.lastFinalSoc = 60.0;
        input.lastEndTime = input.now;
        input.intervalDays = {10.0, 10.0};
        input.intervalDailyKwh = {10.0, 10.0};
        input.last7DayKwh = 70.0;
        const auto forecast = ncs::ChargeForecastService::compute(input);
        if (expectNear(forecast.remainingDays, 2.4, 1e-6) != 0
            || forecast.method != QStringLiteral("电量法"))
            return 4;
    }

    // 5) 长时间未充电：SoC 衰减至 0，剩余 0 天
    {
        auto input = baseInput();
        input.completedCount = 3;
        input.lastFinalSoc = 80.0;
        input.lastEndTime = input.now.addDays(-30);
        input.intervalDays = {2.0, 3.0};
        input.intervalDailyKwh = {10.0, 10.0};
        input.last7DayKwh = 0.0;
        const auto forecast = ncs::ChargeForecastService::compute(input);
        if (expectNear(forecast.currentSoc, 0.0, 1e-6) != 0
            || expectNear(forecast.remainingDays, 0.0, 1e-6) != 0)
            return 5;
    }

    return 0;
}
