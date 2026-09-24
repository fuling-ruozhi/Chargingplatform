#include "business_route_json.h"

#include "service/user_service.h"

#include <QJsonArray>

namespace ncs {

QJsonObject userJson(const User &user)
{
    return {{QStringLiteral("id"), user.id},
            {QStringLiteral("phone_masked"), UserService::maskedPhone(user.phone)},
            {QStringLiteral("nickname"), user.nickname},
            {QStringLiteral("avatar_path"), user.avatarPath},
            {QStringLiteral("balance"), user.balance},
            {QStringLiteral("status"), user.status},
            {QStringLiteral("created_at"), user.createdAt}};
}

QJsonObject legacyUserJson(const User &user)
{
    QJsonObject data = userJson(user);
    data.insert(QStringLiteral("username"), user.username);
    return data;
}

QJsonObject userPreferenceJson(const UserPreference &preference)
{
    QJsonArray types;
    for (const int type : preference.preferredChargerTypes) types.append(type);
    return {{QStringLiteral("home_latitude"), preference.hasHomeLocation
                 ? QJsonValue(preference.homeLatitude) : QJsonValue(QJsonValue::Null)},
            {QStringLiteral("home_longitude"), preference.hasHomeLocation
                 ? QJsonValue(preference.homeLongitude) : QJsonValue(QJsonValue::Null)},
            {QStringLiteral("home_radius_km"), preference.homeRadiusKm},
            {QStringLiteral("preferred_charger_types"), types},
            {QStringLiteral("reminder_start_time"), preference.reminderStartTime},
            {QStringLiteral("reminder_end_time"), preference.reminderEndTime},
            {QStringLiteral("min_idle_chargers"), preference.minIdleChargers},
            {QStringLiteral("dnd_start_time"), preference.dndStartTime},
            {QStringLiteral("dnd_end_time"), preference.dndEndTime},
            {QStringLiteral("enabled"), preference.enabled},
            {QStringLiteral("updated_at"), preference.updatedAt}};
}

QJsonObject reminderMatchJson(const ReminderMatch &match)
{
    QJsonArray types;
    for (const int type : match.matchedTypes) types.append(type);
    QJsonObject result{{QStringLiteral("station_id"), match.stationId},
                       {QStringLiteral("station_name"), match.stationName},
                       {QStringLiteral("idle_chargers"), match.idleMatchedChargers},
                       {QStringLiteral("matched_types"), types},
                       {QStringLiteral("is_favorite"), match.favorite},
                       {QStringLiteral("inside_home_radius"), match.insideHomeRadius},
                       {QStringLiteral("reason"), match.reason}};
    result.insert(QStringLiteral("distance_km"), match.hasDistance
                      ? QJsonValue(match.distanceKm) : QJsonValue(QJsonValue::Null));
    return result;
}

QJsonObject vehicleProfileJson(const VehicleProfile &profile)
{
    return {{QStringLiteral("battery_capacity_kwh"), profile.batteryCapacityKwh},
            {QStringLiteral("target_soc"), profile.targetSoc},
            {QStringLiteral("min_balance_reserve"), profile.minBalanceReserve},
            {QStringLiteral("usual_leave_time"), profile.usualLeaveTime},
            {QStringLiteral("charge_mode"), profile.chargeMode},
            {QStringLiteral("updated_at"), profile.updatedAt}};
}

}
