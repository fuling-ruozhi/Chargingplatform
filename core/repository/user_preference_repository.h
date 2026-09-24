#pragma once

#include "model/user_preference.h"

#include <QVector>

namespace ncs {

class DatabaseManager;

class UserPreferenceRepository
{
public:
    explicit UserPreferenceRepository(DatabaseManager &database);

    bool getByUserId(qint64 userId, UserPreference *preference, bool *found,
                     QString *error) const;
    bool saveWithTypes(const UserPreference &preference, QString *error) const;
    bool preferredTypes(qint64 userId, QVector<int> *types, QString *error) const;

private:
    DatabaseManager &database_;
};

}
