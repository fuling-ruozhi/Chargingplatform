#pragma once

class QSqlDatabase;
class QString;

namespace ncs {

class SchemaInitializer
{
public:
    static bool apply(QSqlDatabase &database, QString *error);
};

}  // namespace ncs
