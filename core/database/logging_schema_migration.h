#pragma once
class QSqlDatabase; class QString;
namespace ncs { bool migrateLoggingSchema(QSqlDatabase &, QString *); }
