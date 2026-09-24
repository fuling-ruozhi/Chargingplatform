#include "preference_schema_migration.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace ncs {
namespace {

bool exec(QSqlDatabase &database, const char *sql, QString *error)
{
    QSqlQuery query(database);
    if (query.exec(QString::fromLatin1(sql))) return true;
    *error = query.lastError().text();
    return false;
}

}

bool migratePreferenceSchemaV7ToV8(QSqlDatabase &database, QString *error)
{
    return exec(database,
        "CREATE TABLE IF NOT EXISTS user_preference("
        "user_id INTEGER PRIMARY KEY,home_latitude REAL,home_longitude REAL,"
        "home_radius_km REAL NOT NULL DEFAULT 3.0 CHECK(home_radius_km>0 AND home_radius_km<=100),"
        "reminder_start_time TEXT NOT NULL DEFAULT '08:00',"
        "reminder_end_time TEXT NOT NULL DEFAULT '22:00',"
        "min_idle_chargers INTEGER NOT NULL DEFAULT 1 CHECK(min_idle_chargers>=1),"
        "dnd_start_time TEXT NOT NULL DEFAULT '22:00',"
        "dnd_end_time TEXT NOT NULL DEFAULT '07:00',"
        "enabled INTEGER NOT NULL DEFAULT 1 CHECK(enabled IN(0,1)),"
        "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "CHECK((home_latitude IS NULL AND home_longitude IS NULL) OR "
        "(home_latitude BETWEEN -90 AND 90 AND home_longitude BETWEEN -180 AND 180)),"
        "FOREIGN KEY(user_id) REFERENCES user(id) ON UPDATE CASCADE ON DELETE CASCADE)", error)
        && exec(database,
        "CREATE TABLE IF NOT EXISTS user_favorite_station("
        "user_id INTEGER NOT NULL,station_id INTEGER NOT NULL,"
        "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "PRIMARY KEY(user_id,station_id),"
        "FOREIGN KEY(user_id) REFERENCES user(id) ON UPDATE CASCADE ON DELETE CASCADE,"
        "FOREIGN KEY(station_id) REFERENCES station(id) ON UPDATE CASCADE ON DELETE CASCADE)", error)
        && exec(database,
        "CREATE INDEX IF NOT EXISTS idx_favorite_station_station_id "
        "ON user_favorite_station(station_id)", error)
        && exec(database,
        "CREATE TABLE IF NOT EXISTS user_preferred_charger_type("
        "user_id INTEGER NOT NULL,charger_type INTEGER NOT NULL CHECK(charger_type IN(0,1)),"
        "PRIMARY KEY(user_id,charger_type),"
        "FOREIGN KEY(user_id) REFERENCES user(id) ON UPDATE CASCADE ON DELETE CASCADE)", error);
}

}
