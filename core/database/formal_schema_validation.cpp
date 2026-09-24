#include "formal_schema_validation.h"

#include <QSet>
#include <QSqlQuery>
#include <QSqlError>

namespace ncs {

bool requireColumns(QSqlDatabase &database, const QString &table,
                    const QStringList &required, QString *error)
{
    if (!database.tables().contains(table)) {
        *error = QStringLiteral("Required table is missing: %1").arg(table);
        return false;
    }
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
        *error = query.lastError().text();
        return false;
    }
    QSet<QString> found;
    while (query.next()) found.insert(query.value(1).toString());
    for (const QString &column : required) {
        if (!found.contains(column)) {
            *error = QStringLiteral("Required column is missing: %1.%2")
                         .arg(table, column);
            return false;
        }
    }
    return true;
}

}
