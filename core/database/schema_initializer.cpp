#include "schema_initializer.h"

#include <QFile>
#include <QResource>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>

static void initializeDatabaseResources()
{
    Q_INIT_RESOURCE(database);
}

namespace ncs {

bool SchemaInitializer::apply(QSqlDatabase &database, QString *error)
{
    initializeDatabaseResources();
    QFile schemaFile(QStringLiteral(":/db/schema.sql"));
    if (!schemaFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *error = QStringLiteral("Cannot open embedded database schema.");
        return false;
    }

    const QString schema = QString::fromUtf8(schemaFile.readAll());
    const QStringList statements = schema.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &rawStatement : statements) {
        const QString statement = rawStatement.trimmed();
        if (statement.isEmpty()) {
            continue;
        }
        QSqlQuery query(database);
        if (!query.exec(statement)) {
            *error = QStringLiteral("Schema statement failed: %1")
                         .arg(query.lastError().text());
            return false;
        }
    }
    return true;
}

}  // namespace ncs
