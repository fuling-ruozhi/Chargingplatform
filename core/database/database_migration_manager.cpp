#include "database_migration_manager.h"

#include "formal_schema_migration.h"
#include "preference_schema_migration.h"
#include "user_schema_migration.h"
#include "logging_schema_migration.h"

#include "util/password_hasher.h"

#include <QDateTime>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QUuid>

namespace ncs {
namespace {

struct ColumnInfo { bool notNull = false; QString defaultValue; };

bool execute(QSqlDatabase &database, const QString &sql, QString *error)
{
    QSqlQuery query(database);
    if (query.exec(sql)) return true;
    *error = QStringLiteral("Migration SQL failed: %1; SQL: %2")
                 .arg(query.lastError().text(), sql);
    return false;
}

bool tableExists(QSqlDatabase &database, const QString &table)
{
    return database.tables().contains(table);
}

bool safeIdentifier(const QString &identifier)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    return pattern.match(identifier).hasMatch();
}

bool columns(QSqlDatabase &database, const QString &table,
             QMap<QString, ColumnInfo> *result, QString *error)
{
    if (!safeIdentifier(table)) {
        *error = QStringLiteral("Unsafe table name: %1").arg(table);
        return false;
    }
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
        *error = QStringLiteral("Cannot inspect %1: %2").arg(table, query.lastError().text());
        return false;
    }
    result->clear();
    while (query.next())
        result->insert(query.value(1).toString(),
                       {query.value(3).toInt() != 0, query.value(4).toString()});
    return true;
}

bool addColumn(QSqlDatabase &database, const QString &table,
               QMap<QString, ColumnInfo> *existing, const QString &name,
               const QString &definition, QString *error)
{
    if (!safeIdentifier(table) || !safeIdentifier(name)) {
        *error = QStringLiteral("Unsafe migration identifier: %1.%2").arg(table, name);
        return false;
    }
    if (existing->contains(name)) return true;
    if (!execute(database, QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3")
                               .arg(table, name, definition), error)) return false;
    return columns(database, table, existing, error);
}

bool recordVersion(QSqlDatabase &database, int version, QString *error)
{
    QSqlQuery query(database);
    if (!query.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO schema_version(version,applied_at) VALUES(:version,:time)"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":version"), version);
    query.bindValue(QStringLiteral(":time"),
                    QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    if (query.exec()) return true;
    *error = query.lastError().text();
    return false;
}

bool rebuildUserIfBlocked(QSqlDatabase &database, const QMap<QString, ColumnInfo> &existing,
                          QString *error)
{
    const bool phoneBlocked = existing.contains(QStringLiteral("phone"))
        && existing.value(QStringLiteral("phone")).notNull
        && existing.value(QStringLiteral("phone")).defaultValue.isEmpty();
    const bool passwordBlocked = existing.contains(QStringLiteral("password"))
        && existing.value(QStringLiteral("password")).notNull
        && existing.value(QStringLiteral("password")).defaultValue.isEmpty();
    if (!phoneBlocked && !passwordBlocked) return execute(database,
        QStringLiteral("CREATE UNIQUE INDEX IF NOT EXISTS idx_user_username ON user(username)"), error);

    QStringList definitions{
        QStringLiteral("id INTEGER PRIMARY KEY AUTOINCREMENT"),
        QStringLiteral("username TEXT NOT NULL UNIQUE"),
        QStringLiteral("password_hash TEXT NOT NULL DEFAULT ''"),
        QStringLiteral("salt TEXT NOT NULL DEFAULT ''"),
        QStringLiteral("nickname TEXT NOT NULL DEFAULT ''"),
        QStringLiteral("created_at TEXT NOT NULL DEFAULT ''")};
    QStringList copied{QStringLiteral("id"), QStringLiteral("username"),
                       QStringLiteral("password_hash"), QStringLiteral("salt"),
                       QStringLiteral("nickname"), QStringLiteral("created_at")};
    if (existing.contains(QStringLiteral("phone"))) {
        definitions.append(QStringLiteral("phone TEXT UNIQUE DEFAULT NULL")); copied.append(QStringLiteral("phone"));
    }
    if (existing.contains(QStringLiteral("avatar_path"))) {
        definitions.append(QStringLiteral("avatar_path TEXT NOT NULL DEFAULT ''")); copied.append(QStringLiteral("avatar_path"));
    }
    if (existing.contains(QStringLiteral("balance"))) {
        definitions.append(QStringLiteral("balance REAL NOT NULL DEFAULT 0")); copied.append(QStringLiteral("balance"));
    }
    if (existing.contains(QStringLiteral("status"))) {
        definitions.append(QStringLiteral("status INTEGER NOT NULL DEFAULT 1")); copied.append(QStringLiteral("status"));
    }
    if (existing.contains(QStringLiteral("password"))) {
        definitions.append(QStringLiteral("password TEXT DEFAULT NULL")); copied.append(QStringLiteral("password"));
    }
    return execute(database, QStringLiteral("DROP TABLE IF EXISTS user_phase3"), error)
        && execute(database, QStringLiteral("CREATE TABLE user_phase3(%1)").arg(definitions.join(',')), error)
        && execute(database, QStringLiteral("INSERT INTO user_phase3(%1) SELECT %1 FROM user")
                                 .arg(copied.join(',')), error)
        && execute(database, QStringLiteral("DROP TABLE user"), error)
        && execute(database, QStringLiteral("ALTER TABLE user_phase3 RENAME TO user"), error)
        && execute(database, QStringLiteral("CREATE UNIQUE INDEX IF NOT EXISTS idx_user_username ON user(username)"), error);
}

bool migrateUsers(QSqlDatabase &database, QString *error)
{
    if (!tableExists(database, QStringLiteral("user"))
        && !execute(database, QStringLiteral(
            "CREATE TABLE user(id INTEGER PRIMARY KEY AUTOINCREMENT,username TEXT NOT NULL UNIQUE,"
            "password_hash TEXT NOT NULL DEFAULT '',salt TEXT NOT NULL DEFAULT '',"
            "nickname TEXT NOT NULL DEFAULT '',created_at TEXT NOT NULL DEFAULT '')"), error)) return false;
    QMap<QString, ColumnInfo> existing;
    if (!columns(database, QStringLiteral("user"), &existing, error)
        || !addColumn(database, QStringLiteral("user"), &existing, QStringLiteral("username"),
                      QStringLiteral("TEXT NOT NULL DEFAULT ''"), error)
        || !addColumn(database, QStringLiteral("user"), &existing, QStringLiteral("password_hash"),
                      QStringLiteral("TEXT NOT NULL DEFAULT ''"), error)
        || !addColumn(database, QStringLiteral("user"), &existing, QStringLiteral("salt"),
                      QStringLiteral("TEXT NOT NULL DEFAULT ''"), error)
        || !addColumn(database, QStringLiteral("user"), &existing, QStringLiteral("nickname"),
                      QStringLiteral("TEXT NOT NULL DEFAULT ''"), error)
        || !addColumn(database, QStringLiteral("user"), &existing, QStringLiteral("created_at"),
                      QStringLiteral("TEXT NOT NULL DEFAULT ''"), error)) return false;

    QStringList selected{QStringLiteral("id"), QStringLiteral("username"),
                         QStringLiteral("password_hash"), QStringLiteral("salt"),
                         QStringLiteral("nickname"), QStringLiteral("created_at")};
    if (existing.contains(QStringLiteral("phone"))) selected.append(QStringLiteral("phone"));
    if (existing.contains(QStringLiteral("password"))) selected.append(QStringLiteral("password"));
    QSqlQuery select(database);
    if (!select.exec(QStringLiteral("SELECT %1 FROM user ORDER BY id").arg(selected.join(',')))) {
        *error = select.lastError().text(); return false;
    }
    struct Row { qint64 id; QString username, hash, salt, nickname, created; };
    QList<Row> rows; QSet<QString> used;
    while (select.next()) {
        Row row; row.id = select.value(0).toLongLong(); row.username = select.value(1).toString().trimmed();
        if (row.username.isEmpty() && existing.contains(QStringLiteral("phone"))) row.username = select.value(6).toString().trimmed();
        if (row.username.isEmpty() || used.contains(row.username)) row.username = QStringLiteral("legacy_user_%1").arg(row.id);
        while (used.contains(row.username)) row.username.append('_'); used.insert(row.username);
        row.hash = select.value(2).toString(); row.salt = select.value(3).toString();
        if (row.hash.isEmpty() || row.salt.isEmpty()) {
            QString password;
            if (existing.contains(QStringLiteral("password"))) password = select.value(existing.contains(QStringLiteral("phone")) ? 7 : 6).toString();
            if (password.isEmpty()) password = QUuid::createUuid().toString(QUuid::WithoutBraces);
            row.salt = PasswordHasher::makeSalt(); row.hash = PasswordHasher::hash(password, row.salt);
        }
        row.nickname = select.value(4).toString(); if (row.nickname.isEmpty()) row.nickname = row.username;
        row.created = select.value(5).toString(); if (row.created.isEmpty()) row.created = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        rows.append(row);
    }
    QSqlQuery update(database);
    if (!update.prepare(QStringLiteral("UPDATE user SET username=:u,password_hash=:h,salt=:s,nickname=:n,created_at=:c WHERE id=:id"))) {
        *error = update.lastError().text(); return false;
    }
    for (const Row &row : rows) {
        update.bindValue(":u",row.username); update.bindValue(":h",row.hash); update.bindValue(":s",row.salt);
        update.bindValue(":n",row.nickname); update.bindValue(":c",row.created); update.bindValue(":id",row.id);
        if (!update.exec()) { *error = update.lastError().text(); return false; }
    }
    return rebuildUserIfBlocked(database, existing, error);
}

bool migrateCatalog(QSqlDatabase &database, QString *error)
{
    if (!tableExists(database, QStringLiteral("station"))
        && !execute(database, QStringLiteral("CREATE TABLE station(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL DEFAULT '',address TEXT NOT NULL DEFAULT '',price REAL NOT NULL DEFAULT 0,total_slots INTEGER NOT NULL DEFAULT 0)"), error)) return false;
    QMap<QString, ColumnInfo> station;
    if (!columns(database,"station",&station,error)
        || !addColumn(database,"station",&station,"name","TEXT NOT NULL DEFAULT ''",error)
        || !addColumn(database,"station",&station,"address","TEXT NOT NULL DEFAULT ''",error)
        || !addColumn(database,"station",&station,"price","REAL NOT NULL DEFAULT 0",error)
        || !addColumn(database,"station",&station,"total_slots","INTEGER NOT NULL DEFAULT 0",error)) return false;
    if (!tableExists(database, QStringLiteral("charger"))
        && !execute(database, QStringLiteral("CREATE TABLE charger(id INTEGER PRIMARY KEY AUTOINCREMENT,station_id INTEGER NOT NULL DEFAULT 0,code TEXT NOT NULL DEFAULT '',status INTEGER NOT NULL DEFAULT 0)"), error)) return false;
    QMap<QString, ColumnInfo> charger;
    if (!columns(database,"charger",&charger,error)
        || !addColumn(database,"charger",&charger,"station_id","INTEGER NOT NULL DEFAULT 0",error)
        || !addColumn(database,"charger",&charger,"code","TEXT NOT NULL DEFAULT ''",error)
        || !addColumn(database,"charger",&charger,"status","INTEGER NOT NULL DEFAULT 0",error)) return false;
    return execute(database, QStringLiteral("UPDATE station SET total_slots=(SELECT COUNT(*) FROM charger WHERE charger.station_id=station.id) WHERE total_slots=0"), error);
}

bool migrateCharging(QSqlDatabase &database, QString *error)
{
    if (!tableExists(database, QStringLiteral("charging_record"))
        && !execute(database, QStringLiteral("CREATE TABLE charging_record(id INTEGER PRIMARY KEY AUTOINCREMENT,user_id INTEGER NOT NULL DEFAULT 0,charger_id INTEGER NOT NULL DEFAULT 0,start_time TEXT NOT NULL DEFAULT '',end_time TEXT,energy REAL NOT NULL DEFAULT 0)"), error)) return false;
    QMap<QString, ColumnInfo> record;
    if (!columns(database,"charging_record",&record,error)
        || !addColumn(database,"charging_record",&record,"user_id","INTEGER NOT NULL DEFAULT 0",error)
        || !addColumn(database,"charging_record",&record,"charger_id","INTEGER NOT NULL DEFAULT 0",error)
        || !addColumn(database,"charging_record",&record,"start_time","TEXT NOT NULL DEFAULT ''",error)
        || !addColumn(database,"charging_record",&record,"end_time","TEXT DEFAULT ''",error)
        || !addColumn(database,"charging_record",&record,"energy","REAL NOT NULL DEFAULT 0",error)) return false;
    return execute(database, QStringLiteral("CREATE UNIQUE INDEX IF NOT EXISTS idx_one_active_record_per_charger ON charging_record(charger_id) WHERE end_time IS NULL"), error);
}

bool migrateChargingSettlement(QSqlDatabase &database, QString *error)
{
    QMap<QString, ColumnInfo> record;
    return columns(database,"charging_record",&record,error)
        && addColumn(database,"charging_record",&record,"cost","REAL NOT NULL DEFAULT 0",error);
}

bool migrateReservationOrders(QSqlDatabase &database, QString *error)
{
    QMap<QString, ColumnInfo> charger;
    if (!columns(database, QStringLiteral("charger"), &charger, error)
        || !addColumn(database, QStringLiteral("charger"), &charger,
                      QStringLiteral("power_kw"),
                      QStringLiteral("REAL NOT NULL DEFAULT 0"), error)) {
        return false;
    }

    QMap<QString, ColumnInfo> record;
    if (!columns(database, QStringLiteral("charging_record"), &record, error)
        || !addColumn(database, QStringLiteral("charging_record"), &record,
                      QStringLiteral("status"),
                      QStringLiteral("INTEGER NOT NULL DEFAULT 2 CHECK(status IN (0,1,2,3))"),
                      error)
        || !addColumn(database, QStringLiteral("charging_record"), &record,
                      QStringLiteral("reserved_at"),
                      QStringLiteral("TEXT NOT NULL DEFAULT ''"), error)
        || !addColumn(database, QStringLiteral("charging_record"), &record,
                      QStringLiteral("expire_at"),
                      QStringLiteral("TEXT NOT NULL DEFAULT ''"), error)) {
        return false;
    }

    if (!execute(database, QStringLiteral(
            "UPDATE charging_record "
            "SET status=CASE WHEN end_time IS NULL THEN 1 ELSE 2 END, "
            "reserved_at=CASE WHEN reserved_at='' THEN start_time ELSE reserved_at END "
            "WHERE status=2"), error)) {
        return false;
    }

    return execute(database, QStringLiteral(
               "CREATE UNIQUE INDEX IF NOT EXISTS idx_one_active_order_per_user "
               "ON charging_record(user_id) WHERE status IN (0,1)"), error)
        && execute(database, QStringLiteral(
               "CREATE UNIQUE INDEX IF NOT EXISTS idx_one_active_order_per_charger "
               "ON charging_record(charger_id) WHERE status IN (0,1)"), error);
}

bool migrateVehicleProfile(QSqlDatabase &database, QString *error)
{
    if (tableExists(database, QStringLiteral("vehicle_profile"))) return true;
    return execute(database, QStringLiteral(
        "CREATE TABLE vehicle_profile("
        "user_id INTEGER PRIMARY KEY,"
        "battery_capacity_kwh REAL NOT NULL DEFAULT 60 "
        "CHECK(battery_capacity_kwh BETWEEN 1 AND 200),"
        "target_soc REAL NOT NULL DEFAULT 80 CHECK(target_soc BETWEEN 10 AND 100),"
        "min_balance_reserve REAL NOT NULL DEFAULT 5 CHECK(min_balance_reserve >= 0),"
        "usual_leave_time TEXT NOT NULL DEFAULT '18:00' CHECK(length(usual_leave_time)=5),"
        "charge_mode INTEGER NOT NULL DEFAULT 0 CHECK(charge_mode IN (0,1,2)),"
        "updated_at TEXT NOT NULL DEFAULT '',"
        "FOREIGN KEY(user_id) REFERENCES user(id) "
        "ON UPDATE CASCADE ON DELETE CASCADE)"), error);
}

bool foreignKeyCheck(QSqlDatabase &database, QString *error)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA foreign_key_check"))) {
        *error = QStringLiteral("Cannot validate migrated foreign keys: %1")
                     .arg(query.lastError().text());
        return false;
    }
    if (!query.next()) return true;
    *error = QStringLiteral("Migrated database contains an invalid foreign key: %1 row %2")
                 .arg(query.value(0).toString(), query.value(1).toString());
    return false;
}

}

bool DatabaseMigrationManager::migrate(QSqlDatabase &database, int fromVersion, QString *error)
{
    if (fromVersion < 1 || fromVersion > LatestVersion) {
        *error = QStringLiteral("Unsupported schema version %1").arg(fromVersion); return false;
    }
    if (!execute(database, QStringLiteral("PRAGMA foreign_keys=OFF"), error)) return false;
    if (!database.transaction()) { *error = database.lastError().text(); return false; }
    bool ok = FormalSchemaMigration::repairKnownSchemaDrift(database, error);
    if (ok && fromVersion >= 7) ok = migrateVehicleProfile(database, error);
    int version = fromVersion;
    if (version == 1) {
        ok = migrateUsers(database,error) && migrateCatalog(database,error)
            && recordVersion(database,2,error);
        if (ok) version = 2;
    }
    if (ok && version == 2) {
        ok = migrateCharging(database,error) && recordVersion(database,3,error);
        if (ok) version = 3;
    }
    if (ok && version == 3) {
        ok = migrateChargingSettlement(database,error) && recordVersion(database,4,error);
        if (ok) version = 4;
    }
    if (ok && version == 4) {
        ok = migrateReservationOrders(database,error) && recordVersion(database,5,error);
        if (ok) version = 5;
    }
    if (ok && version == 5) {
        ok = FormalSchemaMigration::migrateV5ToV6(database, error)
            && recordVersion(database, 6, error);
        if (ok) version = 6;
    }
    if (ok && version == 6) {
        ok = FormalSchemaMigration::migrateV6ToV7(database, error)
            && recordVersion(database, 7, error);
        if (ok) version = 7;
    }
    if (ok && version == 7) {
        ok = migratePreferenceSchemaV7ToV8(database, error)
            && migrateVehicleProfile(database, error)
            && recordVersion(database, 8, error);
        if (ok) version = 8;
    }
    if (ok && version == 8) {
        ok = migrateUserSchemaV8ToV9(database, error)
            && recordVersion(database, 9, error);
        if (ok) version = 9;
    }
    if (ok && version == 9) {
        ok = migrateLoggingSchema(database, error) && recordVersion(database, 10, error);
        if (ok) version = 10;
    }
    if (ok) ok = validate(database,error) && foreignKeyCheck(database, error);
    if (ok) ok = database.commit(); else database.rollback();
    QSqlQuery pragma(database);
    if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys=ON")) && ok) {
        *error = QStringLiteral("Cannot re-enable foreign keys: %1")
                     .arg(pragma.lastError().text());
        ok = false;
    }
    if (ok && (!pragma.exec(QStringLiteral("PRAGMA foreign_keys")) || !pragma.next()
               || pragma.value(0).toInt() != 1)) {
        *error = QStringLiteral("Foreign-key enforcement is disabled after migration");
        ok = false;
    }
    if (!ok && error->isEmpty()) *error = database.lastError().text();
    return ok;
}

bool DatabaseMigrationManager::validate(QSqlDatabase &database, QString *error)
{
    return FormalSchemaMigration::validate(database, error);
}

}
