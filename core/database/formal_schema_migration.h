#pragma once

class QSqlDatabase;
class QString;

namespace ncs {

class FormalSchemaMigration
{
public:
    static bool migrateV5ToV6(QSqlDatabase &database, QString *error);
    static bool migrateV6ToV7(QSqlDatabase &database, QString *error);
    static bool repairKnownSchemaDrift(QSqlDatabase &database, QString *error);
    static bool validate(QSqlDatabase &database, QString *error);
};

}
