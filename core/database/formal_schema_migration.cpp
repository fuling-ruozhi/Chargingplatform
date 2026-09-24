#include "formal_schema_migration.h"
#include "formal_schema_validation.h"
#include "preference_schema_migration.h"
#include "user_schema_migration.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
namespace ncs {
namespace {
bool execSql(QSqlDatabase &db, const QString &sql, QString *error)
{
    QSqlQuery query(db);
    if (query.exec(sql)) return true;
    *error = QStringLiteral("Formal migration failed: %1; SQL: %2")
                 .arg(query.lastError().text(), sql);
    return false;
}

bool tableExists(QSqlDatabase &db, const QString &table)
{
    return db.tables().contains(table);
}

bool safeIdentifier(const QString &identifier)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    return pattern.match(identifier).hasMatch();
}

bool columnNames(QSqlDatabase &db, const QString &table, QSet<QString> *result,
                 QString *error)
{
    if (!safeIdentifier(table)) {
        *error = QStringLiteral("Unsafe table name: %1").arg(table);
        return false;
    }
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
        *error = query.lastError().text();
        return false;
    }
    result->clear();
    while (query.next()) result->insert(query.value(1).toString());
    return true;
}

bool addColumn(QSqlDatabase &db, const QString &table, QSet<QString> *columns,
               const QString &name, const QString &definition, QString *error)
{
    if (!safeIdentifier(table) || !safeIdentifier(name)) {
        *error = QStringLiteral("Unsafe migration identifier: %1.%2").arg(table, name);
        return false;
    }
    if (columns->contains(name)) return true;
    if (!execSql(db, QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3")
                         .arg(table, name, definition), error)) return false;
    columns->insert(name);
    return true;
}

QString valueOr(const QSet<QString> &columns, const QString &name,
                const QString &fallback, const QString &alias = QStringLiteral("o"))
{
    return columns.contains(name) ? alias + QLatin1Char('.') + name : fallback;
}

bool migrateCoreTables(QSqlDatabase &db, QString *error)
{
    QSet<QString> columns;
    if (!columnNames(db, QStringLiteral("user"), &columns, error)
        || !addColumn(db, "user", &columns, "phone", "TEXT", error)
        || !addColumn(db, "user", &columns, "avatar_path", "TEXT NOT NULL DEFAULT ''", error)
        || !addColumn(db, "user", &columns, "balance", "REAL NOT NULL DEFAULT 0", error)
        || !addColumn(db, "user", &columns, "status", "INTEGER NOT NULL DEFAULT 1", error)
        || !execSql(db, "CREATE UNIQUE INDEX IF NOT EXISTS idx_user_phone ON user(phone)", error)) {
        return false;
    }

    if (!columnNames(db, QStringLiteral("station"), &columns, error)
        || !addColumn(db, "station", &columns, "longitude", "REAL NOT NULL DEFAULT 0", error)
        || !addColumn(db, "station", &columns, "latitude", "REAL NOT NULL DEFAULT 0", error)
        || !addColumn(db, "station", &columns, "created_at", "TEXT NOT NULL DEFAULT ''", error)) {
        return false;
    }

    if (!columnNames(db, QStringLiteral("charger"), &columns, error)
        || !addColumn(db, "charger", &columns, "type", "INTEGER NOT NULL DEFAULT 0", error)
        || !addColumn(db, "charger", &columns, "power_kw", "REAL NOT NULL DEFAULT 7", error)
        || !addColumn(db, "charger", &columns, "total_count", "INTEGER NOT NULL DEFAULT 0", error)
        || !addColumn(db, "charger", &columns, "total_minutes", "INTEGER NOT NULL DEFAULT 0", error)
        || !addColumn(db, "charger", &columns, "created_at", "TEXT NOT NULL DEFAULT ''", error)
        || !execSql(db, "CREATE INDEX IF NOT EXISTS idx_charger_station_id ON charger(station_id)", error)
        || !execSql(db, "CREATE INDEX IF NOT EXISTS idx_charger_status ON charger(status)", error)) {
        return false;
    }
    return true;
}

const char *orderTableSql =
    "CREATE TABLE charging_order("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,order_no TEXT NOT NULL UNIQUE,"
    "user_id INTEGER NOT NULL,charger_id INTEGER NOT NULL,station_id INTEGER NOT NULL,"
    "status INTEGER NOT NULL DEFAULT 0 CHECK(status IN(0,1,2,3)),"
    "reserved_at TEXT NOT NULL DEFAULT '',expire_at TEXT NOT NULL DEFAULT '',"
    "start_time TEXT NOT NULL DEFAULT '',end_time TEXT NOT NULL DEFAULT '',"
    "energy REAL NOT NULL DEFAULT 0 CHECK(energy>=0),"
    "amount REAL NOT NULL DEFAULT 0 CHECK(amount>=0),"
    "debt_amount REAL NOT NULL DEFAULT 0 CHECK(debt_amount>=0),"
    "balance_after REAL NOT NULL DEFAULT 0 CHECK(balance_after>=0),"
    "price_per_kwh REAL NOT NULL DEFAULT 0 CHECK(price_per_kwh>=0),"
    "power_kw REAL NOT NULL DEFAULT 0 CHECK(power_kw>=0),"
    "time_scale INTEGER NOT NULL DEFAULT 60 CHECK(time_scale>0),"
    "initial_soc REAL NOT NULL DEFAULT 20 CHECK(initial_soc BETWEEN 0 AND 100),"
    "final_soc REAL NOT NULL DEFAULT 20 CHECK(final_soc BETWEEN 0 AND 100),"
    "station_name_snapshot TEXT NOT NULL DEFAULT '',"
    "charger_code_snapshot TEXT NOT NULL DEFAULT '',"
    "created_at TEXT NOT NULL DEFAULT '',updated_at TEXT NOT NULL DEFAULT '',"
    "legacy_source TEXT NOT NULL DEFAULT '',legacy_id INTEGER,"
    "FOREIGN KEY(user_id) REFERENCES user(id) ON UPDATE CASCADE ON DELETE RESTRICT,"
    "FOREIGN KEY(charger_id) REFERENCES charger(id) ON UPDATE CASCADE ON DELETE RESTRICT,"
    "FOREIGN KEY(station_id) REFERENCES station(id) ON UPDATE CASCADE ON DELETE RESTRICT)";

bool createOrderIndexes(QSqlDatabase &db, QString *error)
{
    return execSql(db, "CREATE UNIQUE INDEX idx_order_one_active_user ON charging_order(user_id) WHERE status IN(0,1)", error)
        && execSql(db, "CREATE UNIQUE INDEX idx_order_one_active_charger ON charging_order(charger_id) WHERE status IN(0,1)", error)
        && execSql(db, "CREATE UNIQUE INDEX idx_order_legacy_source_id ON charging_order(legacy_source,legacy_id) WHERE legacy_source<>'' AND legacy_id IS NOT NULL", error)
        && execSql(db, "CREATE INDEX idx_order_user_created ON charging_order(user_id,created_at DESC)", error)
        && execSql(db, "CREATE INDEX idx_order_status_start ON charging_order(status,start_time)", error);
}

bool verifyOrderLinks(QSqlDatabase &db, const QString &table, const QSet<QString> &columns,
                      QString *error)
{
    if (!columns.contains("user_id") || !columns.contains("charger_id")
        || !columns.contains("station_id")) {
        QSqlQuery count(db);
        if (!count.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table))
            || !count.next()) {
            *error = count.lastError().text();
            return false;
        }
        if (count.value(0).toLongLong() > 0) {
            *error = QStringLiteral("Legacy charging_order rows lack user/charger/station links");
            return false;
        }
        return true;
    }
    QSqlQuery invalid(db);
    const QString sql = QStringLiteral(
        "SELECT COUNT(*) FROM %1 o LEFT JOIN user u ON u.id=o.user_id "
        "LEFT JOIN charger c ON c.id=o.charger_id "
        "LEFT JOIN station s ON s.id=o.station_id "
        "WHERE u.id IS NULL OR c.id IS NULL OR s.id IS NULL").arg(table);
    if (!invalid.exec(sql) || !invalid.next()) {
        *error = invalid.lastError().text();
        return false;
    }
    if (invalid.value(0).toLongLong() != 0) {
        *error = QStringLiteral("Legacy charging_order contains invalid foreign-key links");
        return false;
    }
    return true;
}

bool rebuildOrders(QSqlDatabase &db, QString *error)
{
    if (!tableExists(db, QStringLiteral("charging_order"))) {
        return execSql(db, QString::fromLatin1(orderTableSql), error)
            && createOrderIndexes(db, error);
    }
    if (tableExists(db, QStringLiteral("charging_order_v5"))) {
        *error = QStringLiteral("Refusing migration: charging_order_v5 already exists");
        return false;
    }

    QSet<QString> c;
    if (!columnNames(db, QStringLiteral("charging_order"), &c, error)
        || !verifyOrderLinks(db, QStringLiteral("charging_order"), c, error)
        || !execSql(db, "ALTER TABLE charging_order RENAME TO charging_order_v5", error)
        || !execSql(db, QString::fromLatin1(orderTableSql), error)) return false;

    const QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    const QString orderNo = QStringLiteral("COALESCE(NULLIF(%1,''),'LEGACY-CO-'||o.id)")
                                .arg(valueOr(c, "order_no", "''"));
    const QString userId = valueOr(c, "user_id", "0");
    const QString chargerId = valueOr(c, "charger_id", "0");
    const QString stationId = valueOr(c, "station_id", "c.station_id");
    const QString status = valueOr(c, "status", "2");
    const QString start = QStringLiteral("COALESCE(%1,'')").arg(valueOr(c, "start_time", "''"));
    const QString end = QStringLiteral("COALESCE(%1,'')").arg(valueOr(c, "end_time", "''"));
    const QString created = QStringLiteral("COALESCE(NULLIF(%1,''),NULLIF(%2,''),NULLIF(%3,''),:now)")
                                .arg(valueOr(c, "created_at", "''"), end, start);
    const QStringList expressions{
        valueOr(c, "id", "NULL"), orderNo, userId, chargerId, stationId, status,
        QStringLiteral("COALESCE(%1,'')").arg(valueOr(c, "reserved_at", start)),
        QStringLiteral("COALESCE(%1,'')").arg(valueOr(c, "expire_at", "''")), start, end,
        QStringLiteral("COALESCE(%1,0)").arg(valueOr(c, "energy", "0")),
        QStringLiteral("COALESCE(%1,0)").arg(valueOr(c, "amount", "0")),
        QStringLiteral("COALESCE(%1,0)").arg(valueOr(c, "debt_amount", "0")),
        QStringLiteral("COALESCE(%1,0)").arg(valueOr(c, "balance_after", "0")),
        QStringLiteral("COALESCE(%1,s.price,0)").arg(valueOr(c, "price_per_kwh", "NULL")),
        QStringLiteral("COALESCE(%1,c.power_kw,0)").arg(valueOr(c, "power_kw", "NULL")),
        QStringLiteral("COALESCE(%1,60)").arg(valueOr(c, "time_scale", "60")),
        QStringLiteral("COALESCE(%1,20)").arg(valueOr(c, "initial_soc", "20")),
        QStringLiteral("COALESCE(%1,20)").arg(valueOr(c, "final_soc", "20")),
        QStringLiteral("COALESCE(NULLIF(%1,''),s.name,'')").arg(valueOr(c, "station_name_snapshot", "''")),
        QStringLiteral("COALESCE(NULLIF(%1,''),c.code,'')").arg(valueOr(c, "charger_code_snapshot", "''")),
        created, QStringLiteral("COALESCE(NULLIF(%1,''),%2)").arg(valueOr(c, "updated_at", "''"), created),
        QStringLiteral("COALESCE(%1,'')").arg(valueOr(c, "legacy_source", "''")),
        valueOr(c, "legacy_id", "NULL")};

    QSqlQuery copy(db);
    const QString sql = QStringLiteral(
        "INSERT INTO charging_order(id,order_no,user_id,charger_id,station_id,status,"
        "reserved_at,expire_at,start_time,end_time,energy,amount,debt_amount,balance_after,"
        "price_per_kwh,power_kw,time_scale,initial_soc,final_soc,station_name_snapshot,"
        "charger_code_snapshot,created_at,updated_at,legacy_source,legacy_id) "
        "SELECT %1 FROM charging_order_v5 o "
        "LEFT JOIN charger c ON c.id=%2 LEFT JOIN station s ON s.id=%3")
            .arg(expressions.join(','), chargerId, stationId);
    if (!copy.prepare(sql)) { *error = copy.lastError().text(); return false; }
    copy.bindValue(QStringLiteral(":now"), now);
    if (!copy.exec()) { *error = copy.lastError().text(); return false; }
    return execSql(db, "DROP TABLE charging_order_v5", error)
        && createOrderIndexes(db, error);
}

bool createSupportingTables(QSqlDatabase &db, QString *error)
{
    const QStringList statements{
        "CREATE TABLE IF NOT EXISTS admin(id INTEGER PRIMARY KEY AUTOINCREMENT,username TEXT NOT NULL UNIQUE,password_hash TEXT NOT NULL,salt TEXT NOT NULL,created_at TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS ops_log(id INTEGER PRIMARY KEY AUTOINCREMENT,charger_id INTEGER,admin_id INTEGER,operation TEXT NOT NULL,detail TEXT NOT NULL DEFAULT '',created_at TEXT NOT NULL,FOREIGN KEY(charger_id) REFERENCES charger(id) ON DELETE SET NULL,FOREIGN KEY(admin_id) REFERENCES admin(id) ON DELETE SET NULL)",
        "CREATE TABLE IF NOT EXISTS load_prediction(id INTEGER PRIMARY KEY AUTOINCREMENT,station_id INTEGER NOT NULL,generated_at TEXT NOT NULL,target_time TEXT NOT NULL,horizon_hours INTEGER NOT NULL DEFAULT 24 CHECK(horizon_hours IN(1,6,24)),predicted_energy REAL NOT NULL,predicted_free_chargers INTEGER NOT NULL,is_peak INTEGER NOT NULL DEFAULT 0,FOREIGN KEY(station_id) REFERENCES station(id) ON DELETE CASCADE)",
        "CREATE TABLE IF NOT EXISTS recharge_log(id INTEGER PRIMARY KEY AUTOINCREMENT,user_id INTEGER NOT NULL,amount REAL NOT NULL,balance_before REAL NOT NULL DEFAULT 0,balance_after REAL NOT NULL DEFAULT 0,created_at TEXT NOT NULL,FOREIGN KEY(user_id) REFERENCES user(id) ON DELETE RESTRICT)",
        "CREATE INDEX IF NOT EXISTS idx_ops_charger_created ON ops_log(charger_id,created_at DESC)",
        "CREATE INDEX IF NOT EXISTS idx_prediction_station_target ON load_prediction(station_id,target_time)",
        "CREATE INDEX IF NOT EXISTS idx_recharge_user_created ON recharge_log(user_id,created_at DESC)"};
    for (const QString &statement : statements) if (!execSql(db, statement, error)) return false;

    QSet<QString> recharge;
    return columnNames(db, QStringLiteral("recharge_log"), &recharge, error)
        && addColumn(db, "recharge_log", &recharge, "balance_before", "REAL NOT NULL DEFAULT 0", error)
        && addColumn(db, "recharge_log", &recharge, "balance_after", "REAL NOT NULL DEFAULT 0", error);
}

bool importChargingRecords(QSqlDatabase &db, QString *error)
{
    if (!tableExists(db, QStringLiteral("charging_record"))) return true;
    QSet<QString> columns;
    if (!columnNames(db, QStringLiteral("charging_record"), &columns, error)) {
        return false;
    }
    QSqlQuery invalid(db);
    if (!invalid.exec(
            "SELECT COUNT(*) FROM charging_record r "
            "LEFT JOIN user u ON u.id=r.user_id "
            "LEFT JOIN charger c ON c.id=r.charger_id "
            "LEFT JOIN station s ON s.id=c.station_id "
            "WHERE u.id IS NULL OR c.id IS NULL OR s.id IS NULL")
        || !invalid.next()) {
        *error = invalid.lastError().text();
        return false;
    }
    if (invalid.value(0).toLongLong() != 0) {
        *error = QStringLiteral("charging_record contains invalid foreign-key links");
        return false;
    }

    QSqlQuery insert(db);
    const QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    const QString status = columns.contains(QStringLiteral("status"))
        ? QStringLiteral("r.status")
        : QStringLiteral("CASE WHEN r.end_time IS NULL THEN 1 ELSE 2 END");
    const QString reservedAt = columns.contains(QStringLiteral("reserved_at"))
        ? QStringLiteral("COALESCE(NULLIF(r.reserved_at,''),r.start_time,'')")
        : QStringLiteral("COALESCE(r.start_time,'')");
    const QString expireAt = columns.contains(QStringLiteral("expire_at"))
        ? QStringLiteral("COALESCE(r.expire_at,'')")
        : QStringLiteral("''");
    const QString cost = columns.contains(QStringLiteral("cost"))
        ? QStringLiteral("COALESCE(r.cost,0)")
        : QStringLiteral("0");
    const QString createdAt = columns.contains(QStringLiteral("created_at"))
        ? QStringLiteral("COALESCE(NULLIF(r.created_at,''),NULLIF(r.start_time,''),:now)")
        : QStringLiteral("COALESCE(NULLIF(r.start_time,''),:now)");
    const QString updatedAt = columns.contains(QStringLiteral("updated_at"))
        ? QStringLiteral("COALESCE(NULLIF(r.updated_at,''),NULLIF(r.end_time,''),"
                         "NULLIF(r.start_time,''),:now)")
        : QStringLiteral("COALESCE(NULLIF(r.end_time,''),NULLIF(r.start_time,''),:now)");
    const QString sql = QStringLiteral(
        "INSERT INTO charging_order(order_no,user_id,charger_id,station_id,status,reserved_at,"
        "expire_at,start_time,end_time,energy,amount,debt_amount,balance_after,price_per_kwh,"
        "power_kw,time_scale,initial_soc,final_soc,station_name_snapshot,charger_code_snapshot,"
        "created_at,updated_at,legacy_source,legacy_id) "
        "SELECT 'MIG-CR-'||r.id,r.user_id,r.charger_id,c.station_id,%1,"
        "%2,%3,"
        "COALESCE(r.start_time,''),COALESCE(r.end_time,''),COALESCE(r.energy,0),"
        "%4,0,0,COALESCE(s.price,0),COALESCE(c.power_kw,0),60,20,20,"
        "COALESCE(s.name,''),COALESCE(c.code,''),%5,%6,"
        "'charging_record',r.id FROM charging_record r "
        "JOIN charger c ON c.id=r.charger_id JOIN station s ON s.id=c.station_id "
        "WHERE NOT EXISTS(SELECT 1 FROM charging_order o "
        "WHERE o.legacy_source='charging_record' AND o.legacy_id=r.id)")
            .arg(status, reservedAt, expireAt, cost, createdAt, updatedAt);
    if (!insert.prepare(sql)) { *error = insert.lastError().text(); return false; }
    insert.bindValue(QStringLiteral(":now"), now);
    if (!insert.exec()) { *error = insert.lastError().text(); return false; }
    return true;
}
}

bool FormalSchemaMigration::migrateV5ToV6(QSqlDatabase &database, QString *error)
{
    return migrateCoreTables(database, error)
        && rebuildOrders(database, error)
        && createSupportingTables(database, error)
        && importChargingRecords(database, error);
}

bool FormalSchemaMigration::migrateV6ToV7(QSqlDatabase &database, QString *error)
{
    if (!execSql(database,
        "CREATE TABLE IF NOT EXISTS charging_review("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "order_id INTEGER NOT NULL UNIQUE,user_id INTEGER NOT NULL,"
        "station_id INTEGER NOT NULL,charger_id INTEGER NOT NULL,"
        "environment_score INTEGER NOT NULL CHECK(environment_score BETWEEN 1 AND 5),"
        "queue_score INTEGER NOT NULL CHECK(queue_score BETWEEN 1 AND 5),"
        "equipment_score INTEGER NOT NULL CHECK(equipment_score BETWEEN 1 AND 5),"
        "parking_score INTEGER NOT NULL CHECK(parking_score BETWEEN 1 AND 5),"
        "overall_score REAL NOT NULL,created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "FOREIGN KEY(order_id) REFERENCES charging_order(id) ON UPDATE CASCADE ON DELETE RESTRICT,"
        "FOREIGN KEY(user_id) REFERENCES user(id) ON UPDATE CASCADE ON DELETE RESTRICT,"
        "FOREIGN KEY(station_id) REFERENCES station(id) ON UPDATE CASCADE ON DELETE RESTRICT,"
        "FOREIGN KEY(charger_id) REFERENCES charger(id) ON UPDATE CASCADE ON DELETE RESTRICT)", error)
        || !execSql(database, "CREATE INDEX IF NOT EXISTS idx_review_station_id ON charging_review(station_id)", error)
        || !execSql(database, "CREATE INDEX IF NOT EXISTS idx_review_user_id ON charging_review(user_id)", error)) {
        return false;
    }
    if (!tableExists(database, QStringLiteral("load_prediction"))
        && !execSql(database,
                    "CREATE TABLE load_prediction(id INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "station_id INTEGER NOT NULL,generated_at TEXT NOT NULL,target_time TEXT NOT NULL,"
                    "horizon_hours INTEGER NOT NULL DEFAULT 24 CHECK(horizon_hours IN(1,6,24)),"
                    "predicted_energy REAL NOT NULL,predicted_free_chargers INTEGER NOT NULL,"
                    "is_peak INTEGER NOT NULL DEFAULT 0,"
                    "FOREIGN KEY(station_id) REFERENCES station(id) ON DELETE CASCADE)", error)) {
        return false;
    }
    return repairKnownSchemaDrift(database, error)
        && execSql(database, "CREATE INDEX IF NOT EXISTS idx_prediction_station_target "
                            "ON load_prediction(station_id,target_time)", error);
}

bool FormalSchemaMigration::repairKnownSchemaDrift(QSqlDatabase &database, QString *error)
{
    if (!tableExists(database, QStringLiteral("load_prediction"))) return true;
    QSet<QString> prediction;
    return columnNames(database, QStringLiteral("load_prediction"), &prediction, error)
        && addColumn(database, "load_prediction", &prediction, "horizon_hours",
                     "INTEGER NOT NULL DEFAULT 24 CHECK(horizon_hours IN(1,6,24))", error);
}

bool FormalSchemaMigration::validate(QSqlDatabase &db, QString *error)
{
    return requireColumns(db, "user", {"id","phone","nickname","avatar_path","balance","status","created_at"}, error)
        && validateCanonicalUserSchema(db, error)
        && requireColumns(db, "admin", {"id","username","password_hash","salt","created_at"}, error)
        && requireColumns(db, "station", {"id","name","address","longitude","latitude","price","total_slots","created_at"}, error)
        && requireColumns(db, "charger", {"id","station_id","code","type","power_kw","status","total_count","total_minutes","created_at"}, error)
        && requireColumns(db, "charging_order", {"id","order_no","user_id","charger_id","station_id","status","reserved_at","expire_at","start_time","end_time","energy","amount","debt_amount","balance_after","price_per_kwh","power_kw","time_scale","initial_soc","final_soc","station_name_snapshot","charger_code_snapshot","created_at","updated_at","legacy_source","legacy_id"}, error)
        && requireColumns(db, "ops_log", {"id","charger_id","admin_id","operation","detail","created_at"}, error)
        && requireColumns(db, "load_prediction", {"id","station_id","generated_at","target_time","horizon_hours","predicted_energy","predicted_free_chargers","is_peak"}, error)
        && requireColumns(db, "recharge_log", {"id","user_id","amount","balance_before","balance_after","created_at"}, error)
        && requireColumns(db, "charging_review", {"id","order_id","user_id","station_id","charger_id",
                                                      "environment_score","queue_score","equipment_score",
                                                      "parking_score","overall_score","created_at"}, error)
        && requireColumns(db, "user_preference", {"user_id","home_latitude","home_longitude",
                                                    "home_radius_km","reminder_start_time",
                                                    "reminder_end_time","min_idle_chargers",
                                                    "dnd_start_time","dnd_end_time","enabled",
                                                    "updated_at"}, error)
        && requireColumns(db, "user_favorite_station", {"user_id","station_id","created_at"}, error)
        && requireColumns(db, "user_preferred_charger_type", {"user_id","charger_type"}, error)
        && requireColumns(db, "vehicle_profile", {"user_id","battery_capacity_kwh","target_soc",
                                                     "min_balance_reserve","usual_leave_time",
                                                     "charge_mode","updated_at"}, error);
}

}
