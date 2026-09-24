#include "database/database_manager.h"
#include "repository/charge_repository.h"
#include "repository/station_repository.h"
#include "repository/user_repository.h"
#include "service/charge_service.h"
#include "service/station_service.h"
#include "service/user_service.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

namespace {

bool createPhase2Database(const QString &path)
{
    const QString name = QStringLiteral("phase2_%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        database.setDatabaseName(path);
        if (database.open()) {
            QSqlQuery query(database);
            ok = query.exec("CREATE TABLE schema_version(version INTEGER PRIMARY KEY,applied_at TEXT NOT NULL)")
                && query.exec("INSERT INTO schema_version VALUES(1,'phase2')")
                && query.exec("CREATE TABLE user(id INTEGER PRIMARY KEY AUTOINCREMENT,phone TEXT NOT NULL UNIQUE,password TEXT NOT NULL,nickname TEXT NOT NULL,avatar_path TEXT NOT NULL DEFAULT '',balance REAL NOT NULL DEFAULT 0,status INTEGER NOT NULL DEFAULT 1,created_at TEXT NOT NULL)")
                && query.exec("INSERT INTO user(phone,password,nickname,created_at) VALUES('legacy01','legacysecret','旧用户','2025-01-01')")
                && query.exec("CREATE TABLE station(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,address TEXT NOT NULL,longitude REAL NOT NULL,latitude REAL NOT NULL,price REAL NOT NULL,created_at TEXT NOT NULL)")
                && query.exec("INSERT INTO station(name,address,longitude,latitude,price,created_at) VALUES('旧版电站','旧地址',116.0,39.0,1.2,'2025-01-01')")
                && query.exec("CREATE TABLE charger(id INTEGER PRIMARY KEY AUTOINCREMENT,station_id INTEGER NOT NULL,code TEXT NOT NULL,type INTEGER NOT NULL,power_kw REAL NOT NULL,status INTEGER NOT NULL,total_count INTEGER NOT NULL DEFAULT 0,total_minutes INTEGER NOT NULL DEFAULT 0,created_at TEXT NOT NULL)")
                && query.exec("INSERT INTO charger(station_id,code,type,power_kw,status,created_at) VALUES(1,'旧版1号桩',0,60.0,0,'2025-01-01')")
                && query.exec("CREATE TABLE charging_order(id INTEGER PRIMARY KEY,user_id INTEGER NOT NULL,station_id INTEGER NOT NULL,charger_id INTEGER NOT NULL,order_no TEXT NOT NULL)")
                && query.exec("INSERT INTO charging_order VALUES(1,1,1,1,'OLD-001')")
                && query.exec("CREATE TABLE reservation(id INTEGER PRIMARY KEY,user_id INTEGER NOT NULL,note TEXT NOT NULL)")
                && query.exec("INSERT INTO reservation VALUES(1,1,'keep-me')");
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(name);
    return ok;
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("phase2.db"));
    if (!createPhase2Database(path)) return 1;
    ncs::DatabaseManager database(path);
    if (!database.initialize() || database.schemaVersion() != 10) return 2;

    ncs::UserRepository userRepository(database);
    ncs::UserService userService(database, userRepository);
    const auto login = userService.login(QStringLiteral("legacy01"),
                                         QStringLiteral("legacysecret"));
    if (!login.success) return 3;
    if (!userService.recharge(login.value.id, 10.0).success) return 13;

    ncs::StationRepository stationRepository(database);
    ncs::StationService stationService(stationRepository);
    const auto stations = stationService.list();
    if (!stations.success || stations.value.size() != 1
        || stations.value.first().totalSlots != 1) return 4;
    const auto detail = stationService.detail(stations.value.first().id);
    if (!detail.success || detail.value.chargers.size() != 1
        || detail.value.chargers.first().code != QStringLiteral("旧版1号桩")) return 5;

    ncs::ChargeRepository chargeRepository(database);
    ncs::ChargeService chargeService(database, chargeRepository);
    const auto started = chargeService.start(login.value.id, detail.value.chargers.first().id);
    if (!started.success) return 6;
    const auto stopped = chargeService.stop(started.value.id);
    if (!stopped.success || stopped.value.endTime.isEmpty()) return 7;

    QSqlQuery preserved(database.connection());
    if (!preserved.exec("SELECT order_no FROM charging_order WHERE id=1")
        || !preserved.next() || preserved.value(0).toString() != QStringLiteral("OLD-001")) return 8;
    if (!preserved.exec("SELECT note FROM reservation WHERE id=1")
        || !preserved.next() || preserved.value(0).toString() != QStringLiteral("keep-me")) return 9;
    if (!preserved.exec("SELECT longitude,latitude,power_kw FROM station JOIN charger ON charger.station_id=station.id WHERE station.id=1")
        || !preserved.next() || preserved.value(0).toDouble() != 116.0
        || preserved.value(1).toDouble() != 39.0 || preserved.value(2).toDouble() != 60.0) return 10;

    QSqlQuery versions(database.connection());
    if (!versions.exec("SELECT group_concat(version, ',') FROM schema_version ORDER BY version")
        || !versions.next()) return 11;
    const QString recorded = versions.value(0).toString();
    if (!recorded.contains('1') || !recorded.contains('2') || !recorded.contains('3')
        || !recorded.contains('4') || !recorded.contains('5')
        || !recorded.contains('6') || !recorded.contains('7') || !recorded.contains('8')
        || !recorded.contains('9') || !recorded.contains("10")) return 12;
    return 0;
}
