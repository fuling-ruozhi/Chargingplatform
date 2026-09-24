#pragma once

#include "model/service_result.h"
#include "model/user_preference.h"

namespace ncs {

class DatabaseManager;
class FavoriteStationRepository;
class UserPreferenceRepository;

class UserPreferenceService
{
public:
    UserPreferenceService(DatabaseManager &database, UserPreferenceRepository &preferences,
                          FavoriteStationRepository &favorites);

    ServiceResult<UserPreference> getPreference(qint64 userId) const;
    ServiceResult<UserPreference> updatePreference(UserPreference preference) const;
    ServiceResult<QVector<qint64>> favoriteStationList(qint64 userId) const;
    ServiceResult<bool> favoriteStationAdd(qint64 userId, qint64 stationId) const;
    ServiceResult<bool> favoriteStationRemove(qint64 userId, qint64 stationId) const;

private:
    static UserPreference defaults(qint64 userId);
    DatabaseManager &database_;
    UserPreferenceRepository &preferences_;
    FavoriteStationRepository &favorites_;
};

}
