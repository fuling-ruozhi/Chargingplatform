#pragma once

#include "model/admin.h"

namespace ncs {

class DatabaseManager;

class AdminRepository
{
public:
    explicit AdminRepository(DatabaseManager &database);

    bool findByUsername(const QString &username, Admin *admin, QString *hash,
                        QString *salt, bool *found, QString *error) const;
    bool chargerCounts(int *online, int *total, QString *error) const;

private:
    DatabaseManager &database_;
};

}
