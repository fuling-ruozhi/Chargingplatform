#include "user_preference_service.h"

#include "model/charger.h"
#include "repository/favorite_station_repository.h"
#include "repository/user_preference_repository.h"
#include "util/geo_distance.h"
#include "util/time_window.h"

#include <QSet>

#include <algorithm>

namespace ncs {

UserPreferenceService::UserPreferenceService(DatabaseManager &database,
                                             UserPreferenceRepository &preferences,
                                             FavoriteStationRepository &favorites)
    : database_(database), preferences_(preferences), favorites_(favorites) {}

UserPreference UserPreferenceService::defaults(qint64 userId)
{
    UserPreference result;
    result.userId = userId;
    return result;
}

ServiceResult<UserPreference> UserPreferenceService::getPreference(qint64 userId) const
{
    if (userId <= 0) {
        return ServiceResult<UserPreference>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("用户编号无效"));
    }
    UserPreference preference = defaults(userId);
    bool found = false;
    QString error;
    if (!preferences_.getByUserId(userId, &preference, &found, &error)
        || (found && !preferences_.preferredTypes(userId, &preference.preferredChargerTypes,
                                                   &error))) {
        return ServiceResult<UserPreference>::fail(BusinessErrorCode::DatabaseError, error);
    }
    return ServiceResult<UserPreference>::ok(preference);
}

ServiceResult<UserPreference> UserPreferenceService::updatePreference(
    UserPreference preference) const
{
    if (preference.userId <= 0) {
        return ServiceResult<UserPreference>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("用户编号无效"));
    }
    const bool hasLatitude = preference.homeLatitudeSet;
    const bool hasLongitude = preference.homeLongitudeSet;
    if (preference.hasHomeLocation && (!hasLatitude || !hasLongitude)) {
        return ServiceResult<UserPreference>::fail(
            BusinessErrorCode::InvalidArgument,
            QStringLiteral("设置家庭位置时必须提供有效经纬度"));
    }
    if (!preference.hasHomeLocation && (hasLatitude || hasLongitude)) {
        return ServiceResult<UserPreference>::fail(
            BusinessErrorCode::InvalidArgument,
            QStringLiteral("设置家庭位置时必须提供有效经纬度"));
    }
    if (hasLatitude != hasLongitude) {
        return ServiceResult<UserPreference>::fail(
            BusinessErrorCode::InvalidArgument,
            QStringLiteral("设置家庭位置时必须提供有效经纬度"));
    }
    if (preference.hasHomeLocation
        && !GeoDistance::validCoordinate(preference.homeLongitude, preference.homeLatitude)) {
        return ServiceResult<UserPreference>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("家庭位置坐标无效"));
    }
    if (preference.homeRadiusKm <= 0.0 || preference.homeRadiusKm > 100.0) {
        return ServiceResult<UserPreference>::fail(
            BusinessErrorCode::InvalidArgument,
            QStringLiteral("附近半径必须大于0且不超过100公里"));
    }
    if (!isValidTimeString(preference.reminderStartTime)
        || !isValidTimeString(preference.reminderEndTime)
        || !isValidTimeString(preference.dndStartTime)
        || !isValidTimeString(preference.dndEndTime)) {
        return ServiceResult<UserPreference>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("时间必须使用 HH:mm 格式"));
    }
    if (preference.minIdleChargers < 1) {
        return ServiceResult<UserPreference>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("最低空闲数必须至少为1"));
    }
    QSet<int> uniqueTypes;
    QVector<int> normalizedTypes;
    for (const int type : preference.preferredChargerTypes) {
        if (!isValidChargerType(type)) {
            return ServiceResult<UserPreference>::fail(
                BusinessErrorCode::InvalidArgument, QStringLiteral("电桩类型无效"));
        }
        if (!uniqueTypes.contains(type)) {
            uniqueTypes.insert(type);
            normalizedTypes.append(type);
        }
    }
    std::sort(normalizedTypes.begin(), normalizedTypes.end());
    preference.preferredChargerTypes = normalizedTypes;
    if (!hasEffectiveReminderTime(preference.reminderStartTime, preference.reminderEndTime,
                                  preference.dndStartTime, preference.dndEndTime)) {
        return ServiceResult<UserPreference>::fail(
            BusinessErrorCode::InvalidArgument,
            QStringLiteral("提醒时段被勿扰时段完全覆盖"));
    }
    QString error;
    if (!preferences_.saveWithTypes(preference, &error)) {
        return ServiceResult<UserPreference>::fail(BusinessErrorCode::DatabaseError, error);
    }
    return ServiceResult<UserPreference>::ok(preference);
}

ServiceResult<QVector<qint64>> UserPreferenceService::favoriteStationList(qint64 userId) const
{
    QVector<qint64> stationIds;
    QString error;
    if (!favorites_.listByUser(userId, &stationIds, &error)) {
        return ServiceResult<QVector<qint64>>::fail(BusinessErrorCode::DatabaseError, error);
    }
    return ServiceResult<QVector<qint64>>::ok(stationIds);
}

ServiceResult<bool> UserPreferenceService::favoriteStationAdd(qint64 userId,
                                                              qint64 stationId) const
{
    if (stationId <= 0) {
        return ServiceResult<bool>::fail(BusinessErrorCode::InvalidArgument,
                                         QStringLiteral("电站编号无效"));
    }
    bool exists = false;
    QString error;
    if (!favorites_.stationExists(stationId, &exists, &error)) {
        return ServiceResult<bool>::fail(BusinessErrorCode::DatabaseError, error);
    }
    if (!exists) {
        return ServiceResult<bool>::fail(BusinessErrorCode::StationNotFound,
                                         QStringLiteral("电站不存在"));
    }
    bool added = false;
    if (!favorites_.add(userId, stationId, &added, &error)) {
        return ServiceResult<bool>::fail(BusinessErrorCode::DatabaseError, error);
    }
    return ServiceResult<bool>::ok(added);
}

ServiceResult<bool> UserPreferenceService::favoriteStationRemove(qint64 userId,
                                                                 qint64 stationId) const
{
    if (stationId <= 0) {
        return ServiceResult<bool>::fail(BusinessErrorCode::InvalidArgument,
                                         QStringLiteral("电站编号无效"));
    }
    bool removed = false;
    QString error;
    if (!favorites_.remove(userId, stationId, &removed, &error)) {
        return ServiceResult<bool>::fail(BusinessErrorCode::DatabaseError, error);
    }
    return ServiceResult<bool>::ok(removed);
}

}
