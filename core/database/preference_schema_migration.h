#pragma once

class QSqlDatabase;
class QString;

namespace ncs {

bool migratePreferenceSchemaV7ToV8(QSqlDatabase &database, QString *error);

}
