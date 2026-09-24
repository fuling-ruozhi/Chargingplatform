#pragma once

#include <QMetaType>
#include <QString>

namespace ncs {

// 充电模式：0 均衡 / 1 省钱 / 2 快速（与 vehicle_profile.charge_mode 一致）
enum class ChargeMode {
    Balanced = 0,
    Economy = 1,
    Fast = 2
};

// UC-EXT-SC-01 车辆档案：每用户一份，存于 vehicle_profile 表。
// 默认值与 ChargeConfig 全局默认保持一致（容量 60kWh、保留余额 5、离开 18:00）。
struct VehicleProfile
{
    qint64 userId = 0;
    double batteryCapacityKwh = 60.0;
    double targetSoc = 80.0;
    double minBalanceReserve = 5.0;
    QString usualLeaveTime = QStringLiteral("18:00");
    int chargeMode = 0;
    QString updatedAt;
};

}

Q_DECLARE_METATYPE(ncs::VehicleProfile)
