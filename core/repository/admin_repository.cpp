#include "admin_repository.h"

#include "database/database_manager.h"

#include <QSqlError>
#include <QSqlQuery>

namespace ncs {

AdminRepository::AdminRepository(DatabaseManager &database) : database_(database) {}

bool AdminRepository::findByUsername(const QString &username, Admin *admin,
                                     QString *hash, QString *salt, bool *found,
                                     QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT id,username,password_hash,salt,created_at "
            "FROM admin WHERE username=:username"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":username"), username);
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    *found = query.next();
    if (*found) {
        admin->id = query.value(0).toLongLong();
        admin->username = query.value(1).toString();
        *hash = query.value(2).toString();
        *salt = query.value(3).toString();
        admin->createdAt = query.value(4).toString();
    }
    return true;
}

bool AdminRepository::chargerCounts(int *online, int *total, QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT COALESCE(SUM(CASE WHEN status IN(0,1) THEN 1 ELSE 0 END),0),"
            "COUNT(*) FROM charger")) || !query.exec() || !query.next()) {
        *error = query.lastError().text();
        return false;
    }
    *online = query.value(0).toInt();
    *total = query.value(1).toInt();
    return true;
}

}
