// UC-EXT-SC-01 车辆档案：UserClientFacade 的 vehicle profile 部分。
// 按仓内 facade 拆分约定（admin_client_facade_analytics/charger/user.cpp）独立成文件，
// 保持 user_client_facade.cpp 不超过 400 行（NFR-M-01）。
#include "user_client_facade.h"

#include "model/vehicle_profile.h"

namespace ncs {

void UserClientFacade::requestVehicleProfile()
{
    sendAuthenticated(QStringLiteral("user.vehicle.profile.get"));
}

void UserClientFacade::updateVehicleProfile(const VehicleProfile &profile)
{
    sendAuthenticated(QStringLiteral("user.vehicle.profile.update"),
                      {{QStringLiteral("battery_capacity_kwh"), profile.batteryCapacityKwh},
                       {QStringLiteral("target_soc"), profile.targetSoc},
                       {QStringLiteral("min_balance_reserve"), profile.minBalanceReserve},
                       {QStringLiteral("usual_leave_time"), profile.usualLeaveTime},
                       {QStringLiteral("charge_mode"), profile.chargeMode}});
}

void UserClientFacade::handleVehicleProfile(const QString &route,
                                            const JsonResponse &response)
{
    VehicleProfile profile;
    profile.batteryCapacityKwh =
        response.data.value(QStringLiteral("battery_capacity_kwh")).toDouble(60.0);
    profile.targetSoc =
        response.data.value(QStringLiteral("target_soc")).toDouble(80.0);
    profile.minBalanceReserve =
        response.data.value(QStringLiteral("min_balance_reserve")).toDouble(5.0);
    profile.usualLeaveTime =
        response.data.value(QStringLiteral("usual_leave_time")).toString();
    profile.chargeMode =
        response.data.value(QStringLiteral("charge_mode")).toInt(0);
    profile.updatedAt =
        response.data.value(QStringLiteral("updated_at")).toString();
    if (route == QStringLiteral("user.vehicle.profile.get")) {
        emit vehicleProfileReceived(profile);
    } else {
        emit vehicleProfileUpdated(profile);
    }
}

}
