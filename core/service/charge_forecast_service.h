#pragma once

#include "model/charge_forecast.h"
#include "model/service_result.h"

#include <QDateTime>
#include <QVector>

namespace ncs {

class ChargingRecord;
class ChargeRepository;
class UserRepository;

// EXT-SC-03 充电间隔分析与续航预测：纯规则引擎，实时计算不落库
class ChargeForecastService
{
public:
    struct Input
    {
        bool hasActiveOrder = false;             // 存在预约/充电中订单
        QDateTime lastEndTime;                   // 上次充电结束时间（本地）
        double lastFinalSoc = 20.0;              // 上次充电结束时 SoC
        QVector<double> intervalDays;            // 近 3~5 次充电间隔（天）
        QVector<double> intervalDailyKwh;        // 与间隔一一对应的单日耗电估计
        double last7DayKwh = 0.0;                // 近 7 天充入总电量（度）
        int completedCount = 0;                  // 已完成订单数
        QDateTime now;                           // 当前本地时间
        double capacityKwh = 60.0;               // 电池容量（车辆档案优先，回退全局配置）
        double defaultDailyKwh = 8.0;            // 冷启动默认日耗电
        double lowSocPercent = 20.0;             // 低电量预警阈值
    };

    ChargeForecastService(ChargeRepository &repository,
                          UserRepository *userRepository = nullptr);

    ServiceResult<ChargeForecast> forecast(qint64 userId) const;

    // 纯计算，独立于数据库，便于单元测试
    static ChargeForecast compute(const Input &input);

private:
    ChargeRepository &repository_;
    UserRepository *userRepository_ = nullptr;  // 读取车辆档案的电池容量（可为空）
};

}
