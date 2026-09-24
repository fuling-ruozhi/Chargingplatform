#include "user_schema_migration.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QString>

namespace ncs {
namespace {

bool execute(QSqlDatabase &database, const QString &sql, QString *error)
{
    QSqlQuery query(database);
    if (query.exec(sql)) return true;
    *error = QStringLiteral("User schema migration failed: %1; SQL: %2")
                 .arg(query.lastError().text(), sql);
    return false;
}

QSet<QString> userColumns(QSqlDatabase &database, QString *error)
{
    QSet<QString> result;
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA table_info(user)"))) {
        *error = query.lastError().text();
        return result;
    }
    while (query.next()) result.insert(query.value(1).toString());
    return result;
}

QString sourceColumn(const QSet<QString> &columns, const QString &name)
{
    return columns.contains(name) ? name : QStringLiteral("NULL");
}

const QString canonicalUserTable = QStringLiteral(
    "CREATE TABLE user_v9("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "phone TEXT UNIQUE,"
    "nickname TEXT NOT NULL DEFAULT '',"
    "avatar_path TEXT NOT NULL DEFAULT '',"
    "balance REAL NOT NULL DEFAULT 0 CHECK(balance >= 0),"
    "status INTEGER NOT NULL DEFAULT 1 CHECK(status IN (0, 1)),"
    "created_at TEXT NOT NULL DEFAULT '',"
    "username TEXT UNIQUE,"
    "password_hash TEXT,"
    "salt TEXT,"
    "CHECK(phone IS NULL OR (length(phone) = 11 AND substr(phone, 1, 1) = '1' "
    "AND phone NOT GLOB '*[^0-9]*'))"
    ")");

}

bool migrateUserSchemaV8ToV9(QSqlDatabase &database, QString *error)
{
    if (!database.tables().contains(QStringLiteral("user"))) {
        *error = QStringLiteral("Cannot normalize missing user table");
        return false;
    }

    const QSet<QString> columns = userColumns(database, error);
    if (columns.isEmpty() && !error->isEmpty()) return false;

    if (!execute(database, QStringLiteral("DROP TABLE IF EXISTS user_v9"), error)
        || !execute(database, canonicalUserTable, error)
        || !execute(database, QStringLiteral(
            "INSERT INTO user_v9(id,phone,nickname,avatar_path,balance,status,created_at,"
            "username,password_hash,salt) "
            "SELECT id,CASE WHEN phone IS NULL OR (length(phone)=11 AND substr(phone,1,1)='1' "
            "AND phone NOT GLOB '*[^0-9]*') THEN phone ELSE NULL END,"
            "COALESCE(nickname,''),COALESCE(avatar_path,''),"
            "COALESCE(balance,0),COALESCE(status,1),COALESCE(created_at,''),"
            "%1,%2,%3 FROM user")
                .arg(sourceColumn(columns, QStringLiteral("username")),
                     sourceColumn(columns, QStringLiteral("password_hash")),
                     sourceColumn(columns, QStringLiteral("salt"))), error)
        || !execute(database, QStringLiteral("DROP TABLE user"), error)
        || !execute(database, QStringLiteral("ALTER TABLE user_v9 RENAME TO user"), error)
        || !execute(database, QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_user_phone ON user(phone)"), error)) {
        return false;
    }
    return true;
}

bool validateCanonicalUserSchema(QSqlDatabase &database, QString *error)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA table_info(user)"))) {
        *error = query.lastError().text();
        return false;
    }
    const QSet<QString> required{
        QStringLiteral("phone"), QStringLiteral("username"),
        QStringLiteral("password_hash"), QStringLiteral("salt")};
    QSet<QString> nullable;
    while (query.next()) {
        if (query.value(3).toInt() == 0) nullable.insert(query.value(1).toString());
    }
    for (const QString &column : required) {
        if (!nullable.contains(column)) {
            *error = QStringLiteral("Canonical user column must be nullable: user.%1")
                         .arg(column);
            return false;
        }
    }
    return true;
}

}
