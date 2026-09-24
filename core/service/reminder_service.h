#pragma once

#include "model/service_result.h"
#include "model/user_preference.h"

#include <QHash>
#include <QMutex>

#include <functional>

namespace ncs {

class DatabaseManager;
class FavoriteStationRepository;
class UserPreferenceRepository;

class ReminderService
{
public:
    ReminderService(DatabaseManager &database, UserPreferenceRepository &preferences,
                    FavoriteStationRepository &favorites,
                    std::function<qint64()> clock = {});

    ServiceResult<QVector<ReminderMatch>> check(qint64 userId,
                                                const QString &currentTime) const;

private:
    DatabaseManager &database_;
    UserPreferenceRepository &preferences_;
    FavoriteStationRepository &favorites_;
    std::function<qint64()> clock_;
    mutable QMutex rateLimitMutex_;
    mutable QHash<qint64, qint64> lastCheckMs_;
};

}
