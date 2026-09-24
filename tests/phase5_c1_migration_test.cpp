#include "database/database_manager.h"
#include "model/charging_record.h"
#include "repository/charge_repository.h"

#include <QCoreApplication>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

namespace {

bool createVersion4Database(const QString &path)
{
    const QString connectionName = QStringLiteral("c1_v4_%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                           connectionName);
        database.setDatabaseName(path);
        if (database.open()) {
            QSqlQuery query(database);
            ok = query.exec(QStringLiteral(
                     "CREATE TABLE schema_version(version INTEGER PRIMARY KEY,applied_at TEXT NOT NULL)"))
                && query.exec(QStringLiteral("INSERT INTO schema_version VALUES(4,'phase4')"))
                && query.exec(QStringLiteral(
                     "CREATE TABLE user(id INTEGER PRIMARY KEY AUTOINCREMENT,username TEXT NOT NULL UNIQUE,password_hash TEXT NOT NULL,salt TEXT NOT NULL,nickname TEXT NOT NULL,created_at TEXT NOT NULL)"))
                && query.exec(QStringLiteral(
                     "INSERT INTO user VALUES(1,'legacy','','','旧用户','2026-01-01')"))
                && query.exec(QStringLiteral(
                     "CREATE TABLE station(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,address TEXT NOT NULL,price REAL NOT NULL,total_slots INTEGER NOT NULL)"))
                && query.exec(QStringLiteral(
                     "INSERT INTO station VALUES(1,'旧电站','旧地址',1.2,2)"))
                && query.exec(QStringLiteral(
                     "CREATE TABLE charger(id INTEGER PRIMARY KEY AUTOINCREMENT,station_id INTEGER NOT NULL,code TEXT NOT NULL UNIQUE,status INTEGER NOT NULL)"))
                && query.exec(QStringLiteral("INSERT INTO charger VALUES(1,1,'OLD-01',1)"))
                && query.exec(QStringLiteral("INSERT INTO charger VALUES(2,1,'OLD-02',0)"))
                && query.exec(QStringLiteral(
                     "CREATE TABLE charging_record(id INTEGER PRIMARY KEY AUTOINCREMENT,user_id INTEGER NOT NULL,charger_id INTEGER NOT NULL,start_time TEXT NOT NULL,end_time TEXT,energy REAL NOT NULL DEFAULT 0,cost REAL NOT NULL DEFAULT 0)"))
                && query.exec(QStringLiteral(
                     "INSERT INTO charging_record VALUES(1,1,1,'2026-01-01T00:00:00.000Z',NULL,0,0)"))
                && query.exec(QStringLiteral(
                     "INSERT INTO charging_record VALUES(2,1,2,'2025-12-01T00:00:00.000Z','2025-12-01T00:10:00.000Z',6,7.2)"))
                && query.exec(QStringLiteral(
                     "CREATE UNIQUE INDEX idx_one_active_record_per_charger ON charging_record(charger_id) WHERE end_time IS NULL"))
                && query.exec(QStringLiteral(
                     "CREATE TABLE charging_order(id INTEGER PRIMARY KEY,order_no TEXT NOT NULL,"
                     "user_id INTEGER NOT NULL,charger_id INTEGER NOT NULL,station_id INTEGER NOT NULL,"
                     "status INTEGER NOT NULL DEFAULT 2)"))
                && query.exec(QStringLiteral(
                     "INSERT INTO charging_order VALUES(9,'LEGACY-KEEP',1,2,1,2)"));
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
    const QString path = directory.filePath(QStringLiteral("phase4.db"));
    if (!createVersion4Database(path)) return 1;

    ncs::DatabaseManager database(path);
    if (!database.initialize() || database.schemaVersion() != 10) return 2;

    QSqlQuery records(database.connection());
    if (!records.exec(QStringLiteral(
            "SELECT legacy_id,status,reserved_at FROM charging_order "
            "WHERE legacy_source='charging_record' ORDER BY legacy_id"))
        || !records.next() || records.value(0).toInt() != 1
        || records.value(1).toInt() != 1
        || records.value(2).toString().isEmpty()
        || !records.next() || records.value(0).toInt() != 2
        || records.value(1).toInt() != 2) return 5;

    ncs::ChargeRepository repository(database);
    ncs::ChargingRecord active;
    bool found = false;
    QString error;
    if (!repository.findActiveForUser(1, &active, &found, &error)
        || !found
        || active.status != ncs::ChargingOrderStatus::Charging) return 6;

    QSqlQuery preserved(database.connection());
    if (!preserved.exec(QStringLiteral("SELECT order_no FROM charging_order WHERE id=9"))
        || !preserved.next()
        || preserved.value(0).toString() != QStringLiteral("LEGACY-KEEP")) return 7;

    if (!database.initialize() || database.schemaVersion() != 10) return 8;
    return 0;
}
