#include "database/database_manager.h"
#include "model/vehicle_profile.h"
#include "repository/user_repository.h"
#include "service/user_service.h"

#include <QCoreApplication>
#include <QTemporaryDir>

namespace {

bool createUser(ncs::UserService &service, qint64 *userId)
{
    const auto otp = service.requestOtp(QStringLiteral("13800138000"));
    if (!otp.success) return false;
    const auto login = service.loginWithOtp(
        QStringLiteral("13800138000"), otp.value.displayCode);
    if (!login.success) return false;
    *userId = login.value.id;
    return true;
}

}

// UC-EXT-SC-01 车辆档案：service 层存取 + 校验 + 非法输入拒绝。
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    const QString path = directory.filePath(QStringLiteral("vehicle_profile.db"));
    ncs::DatabaseManager database(path);
    if (!database.initialize()) return 1;

    ncs::UserRepository repository(database);
    ncs::UserService service(database, repository);

    qint64 userId = 0;
    if (!createUser(service, &userId)) return 2;

    // 未设置时返回与 ChargeConfig 一致的默认档案
    const auto defaults = service.vehicleProfile(userId);
    if (!defaults.success || defaults.value.batteryCapacityKwh != 60.0
        || defaults.value.targetSoc != 80.0
        || defaults.value.minBalanceReserve != 5.0
        || defaults.value.usualLeaveTime != QStringLiteral("18:00")
        || defaults.value.chargeMode != static_cast<int>(ncs::ChargeMode::Balanced)) {
        return 3;
    }

    // 保存自定义档案
    ncs::VehicleProfile profile;
    profile.batteryCapacityKwh = 75.0;
    profile.targetSoc = 90.0;
    profile.minBalanceReserve = 10.0;
    profile.usualLeaveTime = QStringLiteral("07:30");
    profile.chargeMode = static_cast<int>(ncs::ChargeMode::Fast);
    const auto updated = service.updateVehicleProfile(userId, profile);
    if (!updated.success) return 4;

    // 读回与保存一致（upsert 覆盖语义）
    const auto loaded = service.vehicleProfile(userId);
    if (!loaded.success || loaded.value.batteryCapacityKwh != 75.0
        || loaded.value.targetSoc != 90.0
        || loaded.value.minBalanceReserve != 10.0
        || loaded.value.usualLeaveTime != QStringLiteral("07:30")
        || loaded.value.chargeMode != static_cast<int>(ncs::ChargeMode::Fast)) {
        return 5;
    }

    // 非法输入必须被拒绝
    ncs::VehicleProfile bad = profile;
    bad.batteryCapacityKwh = 0.0;
    if (service.updateVehicleProfile(userId, bad).success) return 6;
    bad = profile;
    bad.targetSoc = 200.0;
    if (service.updateVehicleProfile(userId, bad).success) return 7;
    bad = profile;
    bad.usualLeaveTime = QStringLiteral("25:99");
    if (service.updateVehicleProfile(userId, bad).success) return 8;
    bad = profile;
    bad.chargeMode = 9;
    if (service.updateVehicleProfile(userId, bad).success) return 9;

    // 不存在的用户：读写都被拒绝
    if (service.vehicleProfile(99999).success) return 10;
    if (service.updateVehicleProfile(99999, profile).success) return 11;
    return 0;
}
