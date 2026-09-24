#pragma once

class QSqlDatabase;
class QString;

namespace ncs {

class DatabaseMigrationManager
{
public:
    static constexpr int LatestVersion = 10;
    static bool migrate(QSqlDatabase &database, int fromVersion, QString *error);
    static bool validate(QSqlDatabase &database, QString *error);
};

}
