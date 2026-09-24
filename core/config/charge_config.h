#pragma once

namespace ncs {

class ChargeConfig
{
public:
    static double minBalance();
    static int reservationMinutes();
    static int timeScale();
    static double initialSoc();
    static double batteryCapacityKwh();
    // EXT-SC-03 续航预测：冷启动默认日耗电与低电量预警阈值
    static double defaultDailyKwh();
    static double lowSocPercent();
};

}
