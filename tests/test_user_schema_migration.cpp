#include "database/database_manager.h"
#include "repository/user_repository.h"
#include "service/user_service.h"

#include <QCoreApplication>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

namespace {

bool createLegacyDatabase(const QString &path)
{
    const QString connectionName = QStringLiteral("legacy_user_%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(path);
        if (database.open()) {
            QSqlQuery query(database);
            ok = query.exec(QStringLiteral(
                     "CREATE TABLE schema_version(version INTEGER PRIMARY KEY, applied_at TEXT NOT NULL)"))
                && query.exec(QStringLiteral(
                     "INSERT INTO schema_version(version, applied_at) VALUES(1, 'legacy')"))
                && query.exec(QStringLiteral(
                     "CREATE TABLE user(id INTEGER PRIMARY KEY AUTOINCREMENT, password TEXT NOT NULL)"))
                && query.exec(QStringLiteral(
                     "INSERT INTO user(password) VALUES('legacysecret')"));
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return ok;
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("legacy-user.db"));
    if (!createLegacyDatabase(path)) return 1;

    ncs::DatabaseManager database(path);
    if (!database.initialize()) return 2;

    QSet<QString> columns;
    QSqlQuery schema(database.connection());
    if (!schema.exec(QStringLiteral("PRAGMA table_info(user)"))) return 3;
    while (schema.next()) columns.insert(schema.value(1).toString());
    const QSet<QString> required{QStringLiteral("id"), QStringLiteral("username"),
                                 QStringLiteral("password_hash"), QStringLiteral("salt"),
                                 QStringLiteral("nickname"), QStringLiteral("created_at")};
    for (const QString &column : required) {
        if (!columns.contains(column)) return 4;
    }

    QSqlQuery migrated(database.connection());
    if (!migrated.exec(QStringLiteral(
            "SELECT username,password_hash,salt,nickname,created_at FROM user WHERE id=1"))
        || !migrated.next()) return 5;
    if (migrated.value(0).toString() != QStringLiteral("legacy_user_1")
        || migrated.value(1).toString().isEmpty()
        || migrated.value(1).toString() == QStringLiteral("legacysecret")
        || migrated.value(2).toString().isEmpty()
        || migrated.value(3).toString().isEmpty()
        || migrated.value(4).toString().isEmpty()) return 6;

    ncs::UserRepository repository(database);
    ncs::UserService service(database, repository);
    if (!service.login(QStringLiteral("legacy_user_1"), QStringLiteral("legacysecret")).success)
        return 7;
    const auto created = service.registerUser(QStringLiteral("new_user"),
                                              QStringLiteral("secret1"));
    if (!created.success
        || !service.login(QStringLiteral("new_user"), QStringLiteral("secret1")).success)
        return 8;
    return 0;
}
