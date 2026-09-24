#pragma once

class QSqlDatabase;
class QString;

namespace ncs {

bool migrateUserSchemaV8ToV9(QSqlDatabase &database, QString *error);
bool validateCanonicalUserSchema(QSqlDatabase &database, QString *error);

}
