#include "charge_forecast_service.h"

#include "config/charge_config.h"
#include "model/charging_record.h"
#include "model/vehicle_profile.h"
#include "repository/charge_repository.h"
#include "repository/user_repository.h"
#include "util/date_time_storage.h"
#include "util/logger.h"

#include <QDate>
#include <algorithm>
#include <cmath>

namespace ncs {
namespace {

constexpr int kMaxIntervals = 5;
constexpr double kMinGapDays = 0.25;   // 间隔下限，防止同日多次充电放大日耗电

double median(QVector<double> values)
{
    if (values.isEmpty()) return 0.0;
    std::sort(values.begin(), values.end());
    const int middle = values.size() / 2;
    if (values.size() % 2 == 1) return values.at(middle);
    return (values.at(middle - 1) + values.at(middle)) / 2.0;
}

double clampPercent(double value)
{
    return std::clamp(value, 0.0, 100.0);
}

}

ChargeForecastService::ChargeForecastService(ChargeRepository &repository,
                                             UserRepository *userRepository)
    : repository_(repository), userRepository_(userRepository)
{
}

ChargeForecast ChargeForecastService::compute(const Input &input)
{
    ChargeForecast forecast;
    if (input.hasActiveOrder) {
        forecast.charging = true;
        forecast.method = QStringLiteral("充电中");
        forecast.note = QStringLiteral("充电中或预约未结算，暂不进行续航预测");
        return forecast;
    }

    const double dailyDefault = input.defaultDailyKwh;
    const double elapsedDays = input.lastEndTime.isValid()
        ? std::max(0.0, input.lastEndTime.secsTo(input.now) / 86400.0) : 0.0;

    // ① 电量法：日均耗电 = 近 7 日均值与间隔法中位数加权融合
    const double intervalDaily = median(input.intervalDailyKwh);
    const double daily7 = input.last7DayKwh / 7.0;
    double daily = dailyDefault;
    forecast.coldStart = input.completedCount < 2;
    if (input.completedCount >= 2) {
        if (intervalDaily > 0.0 && daily7 > 0.0) {
            daily = (intervalDaily + daily7) / 2.0;
        } else if (intervalDaily > 0.0) {
            daily = intervalDaily;
        } else if (daily7 > 0.0) {
            daily = daily7;
        }
        if (daily <= 0.0) daily = dailyDefault;
    }
    forecast.dailyKwh = daily;

    // 估计当前 SoC：上次 final_soc 按日耗电线性衰减
    const double currentSoc = input.completedCount > 0
        ? clampPercent(input.lastFinalSoc
                       - daily * elapsedDays / input.capacityKwh * 100.0)
        : clampPercent(input.lastFinalSoc);
    forecast.currentSoc = currentSoc;

    // ② 习惯法：近 3~5 次充电间隔中位数 − 已流逝天数
    const double habitGap = median(input.intervalDays);
    const bool habitAvailable = !input.intervalDays.isEmpty();
    const double habitDays = habitAvailable
        ? std::max(0.0, habitGap - elapsedDays) : -1.0;

    // 剩余天数 = min(电量法, 习惯法)
    const double electricDays = daily > 0.0
        ? std::max(0.0, (currentSoc - input.lowSocPercent)
                        / 100.0 * input.capacityKwh / daily) : 0.0;
    if (habitAvailable && habitDays < electricDays) {
        forecast.remainingDays = habitDays;
        forecast.method = QStringLiteral("习惯法");
    } else if (input.completedCount == 0) {
        forecast.remainingDays = electricDays;
        forecast.method = QStringLiteral("默认规则");
    } else {
        forecast.remainingDays = electricDays;
        forecast.method = QStringLiteral("电量法");
    }

    forecast.suggestedDate = input.now.date()
        .addDays(static_cast<qint64>(std::ceil(forecast.remainingDays)))
        .toString(QStringLiteral("yyyy-MM-dd"));
    if (forecast.coldStart) {
        forecast.note = QStringLiteral("历史数据不足，按默认规则估算（日均 %1 度）")
                            .arg(daily, 0, 'f', 1);
    } else {
        forecast.note = QStringLiteral("日均 %1 度，%2 天后建议充电")
                            .arg(daily, 0, 'f', 1)
                            .arg(forecast.remainingDays, 0, 'f', 1);
    }
    return forecast;
}

ServiceResult<ChargeForecast> ChargeForecastService::forecast(qint64 userId) const
{
    if (userId <= 0) {
        return ServiceResult<ChargeForecast>::fail(
            BusinessErrorCode::Forbidden, QStringLiteral("用户身份无效"));
    }

    ChargingRecord active;
    bool hasActive = false;
    QString error;
    if (!repository_.findActiveForUser(userId, &active, &hasActive, &error)) {
        return ServiceResult<ChargeForecast>::fail(
            BusinessErrorCode::DatabaseError, error);
    }

    QVector<ChargingRecord> records;
    if (!repository_.listForUser(userId, 50, 0, &records, &error)) {
        return ServiceResult<ChargeForecast>::fail(
            BusinessErrorCode::DatabaseError, error);
    }

    Input input;
    input.hasActiveOrder = hasActive;
    input.now = DateTimeStorage::now();
    input.capacityKwh = ChargeConfig::batteryCapacityKwh();
    input.defaultDailyKwh = ChargeConfig::defaultDailyKwh();
    input.lowSocPercent = ChargeConfig::lowSocPercent();

    // 车辆档案（UC-EXT-SC-01）的电池容量优先于全局配置
    if (userRepository_) {
        VehicleProfile profile;
        bool found = false;
        if (userRepository_->findVehicleProfile(userId, &profile, &found, &error)
            && found && profile.batteryCapacityKwh > 0.0) {
            input.capacityKwh = profile.batteryCapacityKwh;
        }
        error.clear();
    }

    // listForUser 按 created_at 倒序返回，这里按开始时间升序整理后推导间隔
    QVector<ChargingRecord> completed;
    const QDateTime last7Begin = input.now.addDays(-7);
    for (const ChargingRecord &record : records) {
        if (record.status != ChargingOrderStatus::Completed) continue;
        completed.append(record);
    }
    std::sort(completed.begin(), completed.end(),
              [](const ChargingRecord &a, const ChargingRecord &b) {
                  return DateTimeStorage::fromText(a.startTime)
                       < DateTimeStorage::fromText(b.startTime);
              });
    input.completedCount = completed.size();

    if (!completed.isEmpty()) {
        const ChargingRecord &last = completed.last();
        input.lastEndTime = DateTimeStorage::fromText(
            last.endTime.isEmpty() ? last.startTime : last.endTime);
        input.lastFinalSoc = last.finalSoc;
        for (int i = 1; i < completed.size()
             && input.intervalDays.size() < kMaxIntervals; ++i) {
            const QDateTime previousEnd = DateTimeStorage::fromText(
                completed.at(i - 1).endTime.isEmpty()
                    ? completed.at(i - 1).startTime
                    : completed.at(i - 1).endTime);
            const QDateTime currentStart =
                DateTimeStorage::fromText(completed.at(i).startTime);
            const double gapDays = std::max(
                0.0, previousEnd.secsTo(currentStart) / 86400.0);
            input.intervalDays.append(gapDays);
            input.intervalDailyKwh.append(
                completed.at(i).energy / std::max(gapDays, kMinGapDays));
        }
        for (const ChargingRecord &record : completed) {
            const QDateTime endTime = DateTimeStorage::fromText(
                record.endTime.isEmpty() ? record.startTime : record.endTime);
            if (endTime >= last7Begin && record.energy > 0.0) {
                input.last7DayKwh += record.energy;
            }
        }
    } else {
        // 无历史：按需求 E1，用充电初始 SoC 作为当前估计
        input.lastEndTime = QDateTime();
        input.lastFinalSoc = ChargeConfig::initialSoc();
    }

    return ServiceResult<ChargeForecast>::ok(compute(input));
}

}
