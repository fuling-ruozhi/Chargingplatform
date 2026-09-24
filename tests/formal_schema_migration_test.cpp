#include "database/database_manager.h"
#include "repository/user_repository.h"
#include "service/user_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

namespace {

bool executeAll(QSqlDatabase &database, const QStringList &statements)
{
    QSqlQuery query(database);
    for (const QString &statement : statements) {
        if (!query.exec(statement)) return false;
    }
    return true;
}

bool createVersion5Database(const QString &path)
{
    const QString name = QStringLiteral("formal_v5_%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        database.setDatabaseName(path);
        if (database.open()) {
            ok = executeAll(database, {
                "CREATE TABLE schema_version(version INTEGER PRIMARY KEY,applied_at TEXT NOT NULL)",
                "INSERT INTO schema_version VALUES(5,'phase5-c1')",
                "CREATE TABLE user(id INTEGER PRIMARY KEY AUTOINCREMENT,username TEXT NOT NULL UNIQUE,password_hash TEXT NOT NULL,salt TEXT NOT NULL,nickname TEXT NOT NULL,created_at TEXT NOT NULL)",
                "INSERT INTO user VALUES(1,'legacy-user','hash','salt','旧用户','2026-01-01')",
                "CREATE TABLE station(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,address TEXT NOT NULL,price REAL NOT NULL,total_slots INTEGER NOT NULL)",
                "INSERT INTO station VALUES(1,'旧电站','旧地址',1.5,2)",
                "CREATE TABLE charger(id INTEGER PRIMARY KEY AUTOINCREMENT,station_id INTEGER NOT NULL,code TEXT NOT NULL UNIQUE,status INTEGER NOT NULL,power_kw REAL NOT NULL DEFAULT 7)",
                "INSERT INTO charger VALUES(1,1,'OLD-01',1,60)",
                "INSERT INTO charger VALUES(2,1,'OLD-02',0,7)",
                "CREATE TABLE charging_record(id INTEGER PRIMARY KEY AUTOINCREMENT,user_id INTEGER NOT NULL,charger_id INTEGER NOT NULL,start_time TEXT NOT NULL,end_time TEXT,energy REAL NOT NULL DEFAULT 0,cost REAL NOT NULL DEFAULT 0,status INTEGER NOT NULL DEFAULT 2,reserved_at TEXT NOT NULL DEFAULT '',expire_at TEXT NOT NULL DEFAULT '')",
                "INSERT INTO charging_record VALUES(1,1,1,'2026-01-02 10:00:00',NULL,0,0,1,'2026-01-02 09:55:00','2026-01-02 10:10:00')",
                "INSERT INTO charging_record VALUES(2,1,2,'2026-01-01 10:00:00','2026-01-01 10:30:00',3,4.5,2,'2026-01-01 09:50:00','2026-01-01 10:05:00')",
                "CREATE TABLE charging_order(id INTEGER PRIMARY KEY,order_no TEXT NOT NULL,user_id INTEGER NOT NULL,charger_id INTEGER NOT NULL,station_id INTEGER NOT NULL,status INTEGER NOT NULL)",
                "INSERT INTO charging_order VALUES(9,'LEGACY-ORDER',1,2,1,2)"
            });
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(name);
    return ok;
}

bool createVersion6DatabaseWithoutHorizon(const QString &path)
{
    const QString name = QStringLiteral("formal_v6_%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        database.setDatabaseName(path);
        if (database.open()) {
            ok = executeAll(database, {
                "CREATE TABLE schema_version(version INTEGER PRIMARY KEY,applied_at TEXT NOT NULL)",
                "INSERT INTO schema_version VALUES(6,'formal-v6')",
                "CREATE TABLE user(id INTEGER PRIMARY KEY AUTOINCREMENT,phone TEXT UNIQUE,nickname TEXT NOT NULL DEFAULT '',avatar_path TEXT NOT NULL DEFAULT '',balance REAL NOT NULL DEFAULT 0,status INTEGER NOT NULL DEFAULT 1,created_at TEXT NOT NULL DEFAULT '')",
                "CREATE TABLE admin(id INTEGER PRIMARY KEY AUTOINCREMENT,username TEXT NOT NULL UNIQUE,password_hash TEXT NOT NULL,salt TEXT NOT NULL,created_at TEXT NOT NULL)",
                "CREATE TABLE station(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,address TEXT NOT NULL,longitude REAL NOT NULL DEFAULT 0,latitude REAL NOT NULL DEFAULT 0,price REAL NOT NULL,total_slots INTEGER NOT NULL DEFAULT 0,created_at TEXT NOT NULL DEFAULT '')",
                "INSERT INTO station(id,name,address,price,total_slots) VALUES(1,'预测站','地址',1.5,2)",
                "CREATE TABLE charger(id INTEGER PRIMARY KEY AUTOINCREMENT,station_id INTEGER NOT NULL,code TEXT NOT NULL UNIQUE,type INTEGER NOT NULL DEFAULT 0,power_kw REAL NOT NULL DEFAULT 7,status INTEGER NOT NULL DEFAULT 0,total_count INTEGER NOT NULL DEFAULT 0,total_minutes INTEGER NOT NULL DEFAULT 0,created_at TEXT NOT NULL DEFAULT '')",
                "CREATE TABLE charging_order(id INTEGER PRIMARY KEY AUTOINCREMENT,order_no TEXT NOT NULL UNIQUE,user_id INTEGER NOT NULL,charger_id INTEGER NOT NULL,station_id INTEGER NOT NULL,status INTEGER NOT NULL DEFAULT 0,reserved_at TEXT NOT NULL DEFAULT '',expire_at TEXT NOT NULL DEFAULT '',start_time TEXT NOT NULL DEFAULT '',end_time TEXT NOT NULL DEFAULT '',energy REAL NOT NULL DEFAULT 0,amount REAL NOT NULL DEFAULT 0,debt_amount REAL NOT NULL DEFAULT 0,balance_after REAL NOT NULL DEFAULT 0,price_per_kwh REAL NOT NULL DEFAULT 0,power_kw REAL NOT NULL DEFAULT 0,time_scale INTEGER NOT NULL DEFAULT 60,initial_soc REAL NOT NULL DEFAULT 20,final_soc REAL NOT NULL DEFAULT 20,station_name_snapshot TEXT NOT NULL DEFAULT '',charger_code_snapshot TEXT NOT NULL DEFAULT '',created_at TEXT NOT NULL DEFAULT '',updated_at TEXT NOT NULL DEFAULT '',legacy_source TEXT NOT NULL DEFAULT '',legacy_id INTEGER)",
                "CREATE TABLE ops_log(id INTEGER PRIMARY KEY AUTOINCREMENT,charger_id INTEGER,admin_id INTEGER,operation TEXT NOT NULL,detail TEXT NOT NULL DEFAULT '',created_at TEXT NOT NULL)",
                "CREATE TABLE load_prediction(id INTEGER PRIMARY KEY AUTOINCREMENT,station_id INTEGER NOT NULL,generated_at TEXT NOT NULL,target_time TEXT NOT NULL,predicted_energy REAL NOT NULL,predicted_free_chargers INTEGER NOT NULL,is_peak INTEGER NOT NULL DEFAULT 0)",
                "INSERT INTO load_prediction(station_id,generated_at,target_time,predicted_energy,predicted_free_chargers,is_peak) VALUES(1,'2026-09-06 10:00:00','2026-09-06 11:00:00',12.5,3,0)",
                "CREATE TABLE recharge_log(id INTEGER PRIMARY KEY AUTOINCREMENT,user_id INTEGER NOT NULL,amount REAL NOT NULL,balance_before REAL NOT NULL DEFAULT 0,balance_after REAL NOT NULL DEFAULT 0,created_at TEXT NOT NULL)"
            });
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(name);
    return ok;
}

bool appendInvalidLegacyRecord(const QString &path)
{
    const QString name = QStringLiteral("formal_invalid_%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        database.setDatabaseName(path);
        if (database.open()) {
            QSqlQuery query(database);
            ok = query.exec(QStringLiteral(
                "INSERT INTO charging_record VALUES(3,99,2,'2026-01-03 10:00:00',"
                "'2026-01-03 10:30:00',3,4.5,2,'','')"));
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(name);
    return ok;
}

bool hasColumns(QSqlDatabase database, const QString &table,
                const QSet<QString> &required)
{
    QSet<QString> actual;
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) return false;
    while (query.next()) actual.insert(query.value(1).toString());
    for (const QString &column : required) {
        if (!actual.contains(column)) return false;
    }
    return true;
}

bool columnsAreNullable(QSqlDatabase database, const QString &table,
                        const QSet<QString> &required)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) return false;
    QSet<QString> nullable;
    while (query.next()) {
        if (query.value(3).toInt() == 0) nullable.insert(query.value(1).toString());
    }
    for (const QString &column : required) {
        if (!nullable.contains(column)) return false;
    }
    return true;
}

bool verifyConnectionPragmas(QSqlDatabase database)
{
    QSqlQuery query(database);
    return query.exec(QStringLiteral("PRAGMA foreign_keys")) && query.next()
        && query.value(0).toInt() == 1
        && query.exec(QStringLiteral("PRAGMA journal_mode")) && query.next()
        && query.value(0).toString().compare(QStringLiteral("wal"), Qt::CaseInsensitive) == 0
        && query.exec(QStringLiteral("PRAGMA foreign_key_check")) && !query.next();
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;

    const QString freshPath = directory.filePath(QStringLiteral("fresh.db"));
    ncs::DatabaseManager fresh(freshPath);
    if (!fresh.initialize() || fresh.schemaVersion() != 10) return 2;
    const QSet<QString> expectedTables{
        "user", "admin", "station", "charger", "charging_order",
        "ops_log", "load_prediction", "recharge_log", "charging_review",
        "user_preference", "user_favorite_station", "user_preferred_charger_type"};
    const QStringList tableList = fresh.connection().tables();
    const QSet<QString> actualTables(tableList.cbegin(), tableList.cend());
    for (const QString &table : expectedTables) {
        if (!actualTables.contains(table)) return 3;
    }
    if (actualTables.contains(QStringLiteral("charging_record"))) return 3;
    if (!hasColumns(fresh.connection(), QStringLiteral("charging_order"),
                    {"order_no", "amount", "debt_amount", "price_per_kwh",
                     "power_kw", "initial_soc", "final_soc"})
        || !hasColumns(fresh.connection(), QStringLiteral("load_prediction"),
                       {"station_id", "generated_at", "target_time",
                        "horizon_hours", "predicted_energy",
                        "predicted_free_chargers", "is_peak"})
        || !verifyConnectionPragmas(fresh.connection())) return 4;
    fresh.close();

    const QString oldPath = directory.filePath(QStringLiteral("old.db"));
    if (!createVersion5Database(oldPath)) return 5;
    ncs::DatabaseManager migrated(oldPath);
    if (!migrated.initialize() || migrated.schemaVersion() != 10) return 6;

    const QDir backupDirectory(directory.filePath(QStringLiteral("migration-backups")));
    const QStringList backups = backupDirectory.entryList(
        {QStringLiteral("old-v5-*.db")}, QDir::Files);
    if (backups.size() != 1) return 7;

    QSqlQuery query(migrated.connection());
    if (!query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM charging_order WHERE legacy_source='charging_record'"))
        || !query.next() || query.value(0).toInt() != 2) return 8;
    if (!query.exec(QStringLiteral(
            "SELECT order_no,status,station_name_snapshot,charger_code_snapshot "
            "FROM charging_order WHERE id=9"))
        || !query.next() || query.value(0).toString() != QStringLiteral("LEGACY-ORDER")
        || query.value(1).toInt() != 2
        || query.value(2).toString() != QStringLiteral("旧电站")
        || query.value(3).toString() != QStringLiteral("OLD-02")) return 9;
    if (!hasColumns(migrated.connection(), QStringLiteral("user"),
                    {"phone", "avatar_path", "balance", "status"})
        || !columnsAreNullable(migrated.connection(), QStringLiteral("user"),
                                {"phone", "username", "password_hash", "salt"})
        || !hasColumns(migrated.connection(), QStringLiteral("charger"),
                       {"type", "total_count", "total_minutes", "created_at"})
        || !hasColumns(migrated.connection(), QStringLiteral("load_prediction"),
                       {"horizon_hours"})
        || !verifyConnectionPragmas(migrated.connection())) return 10;

    ncs::UserRepository userRepository(migrated);
    ncs::UserService userService(migrated, userRepository);
    const auto otp = userService.requestOtp(QStringLiteral("13800138099"));
    if (!otp.success
        || !userService.loginWithOtp(QStringLiteral("13800138099"), otp.value.displayCode).success)
        return 14;

    if (!migrated.initialize() || migrated.schemaVersion() != 10) return 11;
    if (!query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM charging_order WHERE legacy_source='charging_record'"))
        || !query.next() || query.value(0).toInt() != 2) return 12;
    if (backupDirectory.entryList({QStringLiteral("old-v5-*.db")}, QDir::Files).size() != 1)
        return 13;

    const QString invalidPath = directory.filePath(QStringLiteral("invalid.db"));
    if (!createVersion5Database(invalidPath) || !appendInvalidLegacyRecord(invalidPath))
        return 15;
    ncs::DatabaseManager invalid(invalidPath);
    if (invalid.initialize() || invalid.schemaVersion() != 5) return 16;
    if (!invalid.lastError().contains(QStringLiteral("invalid foreign-key links"))) return 17;
    if (hasColumns(invalid.connection(), QStringLiteral("user"), {"phone"})) return 18;
    const QDir invalidBackupDirectory(directory.filePath(QStringLiteral("migration-backups")));
    if (invalidBackupDirectory.entryList(
        {QStringLiteral("invalid-v5-*.db")}, QDir::Files).size() != 1) return 19;
    const QString v6Path = directory.filePath(QStringLiteral("v6-no-horizon.db"));
    if (!createVersion6DatabaseWithoutHorizon(v6Path)) return 20;
    ncs::DatabaseManager v6(v6Path);
    if (!v6.initialize() || v6.schemaVersion() != 10) return 21;
    if (!hasColumns(v6.connection(), QStringLiteral("load_prediction"), {"horizon_hours"})) return 22;
    QSqlQuery prediction(v6.connection());
    if (!prediction.exec(QStringLiteral("SELECT horizon_hours FROM load_prediction WHERE station_id=1"))
        || !prediction.next() || prediction.value(0).toInt() != 24) return 23;

    const QString currentDriftPath = directory.filePath(QStringLiteral("current-drift.db"));
    ncs::DatabaseManager currentDriftSeed(currentDriftPath);
    if (!currentDriftSeed.initialize()) return 24;
    currentDriftSeed.close();
    const QString driftConnectionName = QStringLiteral("formal_current_drift_%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase driftDatabase = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), driftConnectionName);
        driftDatabase.setDatabaseName(currentDriftPath);
        if (!driftDatabase.open()) return 25;
        QSqlQuery dropColumn(driftDatabase);
        if (!dropColumn.exec(QStringLiteral(
                "ALTER TABLE load_prediction DROP COLUMN horizon_hours"))) return 26;
        driftDatabase.close();
    }
    QSqlDatabase::removeDatabase(driftConnectionName);
    ncs::DatabaseManager repairedCurrent(currentDriftPath);
    if (!repairedCurrent.initialize() || repairedCurrent.schemaVersion() != 10
        || !hasColumns(repairedCurrent.connection(), QStringLiteral("load_prediction"),
                       {"horizon_hours"})) return 27;
    QSqlQuery repairedPrediction(repairedCurrent.connection());
    if (!repairedPrediction.exec(QStringLiteral(
            "SELECT COUNT(*) FROM load_prediction"))
        || !repairedPrediction.next()) return 28;
    const QDir currentBackupDirectory(directory.filePath(QStringLiteral("migration-backups")));
    if (currentBackupDirectory.entryList({QStringLiteral("current-drift-v10-*.db")},
                                         QDir::Files).size() != 1) return 29;
    return 0;
}
