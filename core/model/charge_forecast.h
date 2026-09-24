#pragma once

#include <QMetaType>
#include <QString>

namespace ncs {

// EXT-SC-03 充电间隔分析与续航预测结果（派生数据，不落库）
struct ChargeForecast
{
    bool charging = false;        // 存在未结算订单（预约/充电中），预测暂停
    bool coldStart = false;       // 历史订单不足，使用默认规则
    double currentSoc = 0.0;      // 估计当前 SoC（0-100）
    double dailyKwh = 0.0;        // 日均耗电（度/天）
    double remainingDays = 0.0;   // 剩余可用天数（充电中为 0）
    QString suggestedDate;        // 建议充电日期 yyyy-MM-dd
    QString method;               // 电量法 / 习惯法 / 默认规则
    QString note;                 // 面向用户的说明文字
};

}

Q_DECLARE_METATYPE(ncs::ChargeForecast)
