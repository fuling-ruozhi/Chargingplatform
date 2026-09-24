#include "user_preference_repository.h"

#include "database/database_manager.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QVariant>

namespace ncs {
namespace {

QString nowUtc()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

bool exec(QSqlQuery &query, QString *error)
{
    if (query.exec()) return true;
    *error = query.lastError().text();
    return false;
}

void readPreference(QSqlQuery &query, UserPreference *preference)
{
    preference->userId = query.value(0).toLongLong();
    preference->hasHomeLocation = !query.value(1).isNull() && !query.value(2).isNull();
    preference->homeLatitudeSet = !query.value(1).isNull();
    preference->homeLongitudeSet = !query.value(2).isNull();
    preference->homeLatitude = query.value(1).toDouble();
    preference->homeLongitude = query.value(2).toDouble();
    preference->homeRadiusKm = query.value(3).toDouble();
    preference->reminderStartTime = query.value(4).toString();
    preference->reminderEndTime = query.value(5).toString();
    preference->minIdleChargers = query.value(6).toInt();
    preference->dndStartTime = query.value(7).toString();
    preference->dndEndTime = query.value(8).toString();
    preference->enabled = query.value(9).toInt() != 0;
    preference->updatedAt = query.value(10).toString();
}

}

UserPreferenceRepository::UserPreferenceRepository(DatabaseManager &database)
    : database_(database) {}

bool UserPreferenceRepository::getByUserId(qint64 userId, UserPreference *preference,
                                           bool *found, QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT user_id,home_latitude,home_longitude,home_radius_km,"
            "reminder_start_time,reminder_end_time,min_idle_chargers,"
            "dnd_start_time,dnd_end_time,enabled,updated_at "
            "FROM user_preference WHERE user_id=:user_id"))) {
        *error = query.lastError().text(); return false;
    }
    query.bindValue(QStringLiteral(":user_id"), userId);
    if (!exec(query, error)) return false;
    *found = query.next();
    if (*found) readPreference(query, preference);
    return true;
}

bool UserPreferenceRepository::preferredTypes(qint64 userId, QVector<int> *types,
                                              QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT charger_type FROM user_preferred_charger_type "
            "WHERE user_id=:user_id ORDER BY charger_type"))) {
        *error = query.lastError().text(); return false;
    }
    query.bindValue(QStringLiteral(":user_id"), userId);
    if (!exec(query, error)) return false;
    types->clear();
    while (query.next()) types->append(query.value(0).toInt());
    return true;
}

bool UserPreferenceRepository::saveWithTypes(const UserPreference &preference,
                                             QString *error) const
{
    if (!database_.transaction()) {
        *error = database_.lastError(); return false;
    }
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "INSERT INTO user_preference(user_id,home_latitude,home_longitude,home_radius_km,"
            "reminder_start_time,reminder_end_time,min_idle_chargers,dnd_start_time,dnd_end_time,"
            "enabled,updated_at) VALUES(:user_id,:home_latitude,:home_longitude,:radius,:start,:end,"
            ":min_idle,:dnd_start,:dnd_end,:enabled,:updated) "
            "ON CONFLICT(user_id) DO UPDATE SET home_latitude=excluded.home_latitude,"
            "home_longitude=excluded.home_longitude,home_radius_km=excluded.home_radius_km,"
            "reminder_start_time=excluded.reminder_start_time,reminder_end_time=excluded.reminder_end_time,"
            "min_idle_chargers=excluded.min_idle_chargers,dnd_start_time=excluded.dnd_start_time,"
            "dnd_end_time=excluded.dnd_end_time,enabled=excluded.enabled,updated_at=excluded.updated_at"))) {
        *error = query.lastError().text(); database_.rollback(); return false;
    }
    query.bindValue(QStringLiteral(":user_id"), preference.userId);
    if (preference.hasHomeLocation) {
        query.bindValue(QStringLiteral(":home_latitude"), preference.homeLatitude);
        query.bindValue(QStringLiteral(":home_longitude"), preference.homeLongitude);
    } else {
        query.bindValue(QStringLiteral(":home_latitude"), QVariant());
        query.bindValue(QStringLiteral(":home_longitude"), QVariant());
    }
    query.bindValue(QStringLiteral(":radius"), preference.homeRadiusKm);
    query.bindValue(QStringLiteral(":start"), preference.reminderStartTime);
    query.bindValue(QStringLiteral(":end"), preference.reminderEndTime);
    query.bindValue(QStringLiteral(":min_idle"), preference.minIdleChargers);
    query.bindValue(QStringLiteral(":dnd_start"), preference.dndStartTime);
    query.bindValue(QStringLiteral(":dnd_end"), preference.dndEndTime);
    query.bindValue(QStringLiteral(":enabled"), preference.enabled ? 1 : 0);
    query.bindValue(QStringLiteral(":updated"), nowUtc());
    if (!exec(query, error)) { database_.rollback(); return false; }

    if (!query.prepare(QStringLiteral(
            "DELETE FROM user_preferred_charger_type WHERE user_id=:user_id"))) {
        *error = query.lastError().text(); database_.rollback(); return false;
    }
    query.bindValue(QStringLiteral(":user_id"), preference.userId);
    if (!exec(query, error)) { database_.rollback(); return false; }
    if (!query.prepare(QStringLiteral(
            "INSERT INTO user_preferred_charger_type(user_id,charger_type) "
            "VALUES(:user_id,:charger_type)"))) {
        *error = query.lastError().text(); database_.rollback(); return false;
    }
    QSet<int> uniqueTypes;
    for (const int type : preference.preferredChargerTypes) {
        if (!uniqueTypes.contains(type)) {
            uniqueTypes.insert(type);
            query.bindValue(QStringLiteral(":user_id"), preference.userId);
            query.bindValue(QStringLiteral(":charger_type"), type);
            if (!exec(query, error)) { database_.rollback(); return false; }
        }
    }
    if (!database_.commit()) {
        *error = database_.lastError(); database_.rollback(); return false;
    }
    return true;
}

}
