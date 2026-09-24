#include "database/database_manager.h"
#include "model/charger.h"
#include "repository/favorite_station_repository.h"
#include "repository/user_preference_repository.h"
#include "service/reminder_service.h"
#include "service/user_preference_service.h"
#include "util/time_window.h"

#include <QCoreApplication>
#include <QSqlQuery>
#include <QTemporaryDir>

#include <limits>

namespace {

bool check(bool condition, const char *message)
{
    if (!condition) qCritical("FAIL: %s", message);
    return condition;
}

bool userIds(QSqlDatabase database, qint64 *first, qint64 *second)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT id FROM user ORDER BY id LIMIT 2"))) return false;
    if (!query.next()) return false;
    *first = query.value(0).toLongLong();
    if (!query.next()) {
        QSqlQuery insert(database);
        if (!insert.prepare(QStringLiteral(
                "INSERT INTO user(phone,nickname,avatar_path,balance,status,created_at,username) "
                "VALUES(:phone,:nickname,'',0,1,:created_at,:username)"))) return false;
        insert.bindValue(QStringLiteral(":phone"), QStringLiteral("13900000001"));
        insert.bindValue(QStringLiteral(":nickname"), QStringLiteral("测试用户2"));
        insert.bindValue(QStringLiteral(":created_at"), QStringLiteral("2026-09-08"));
        insert.bindValue(QStringLiteral(":username"), QStringLiteral("reminder-user-2"));
        if (!insert.exec()) return false;
        *second = insert.lastInsertId().toLongLong();
        return true;
    }
    *second = query.value(0).toLongLong();
    return true;
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::DatabaseManager database(directory.filePath(QStringLiteral("preference.db")));
    if (!check(database.initialize() && database.schemaVersion() == 10, "schema v10 initializes")) return 1;

    qint64 userA = 0;
    qint64 userB = 0;
    if (!check(userIds(database.connection(), &userA, &userB), "seed users exist")) return 2;
    ncs::UserPreferenceRepository preferences(database);
    ncs::FavoriteStationRepository favorites(database);
    ncs::UserPreferenceService preferenceService(database, preferences, favorites);
    qint64 clockMs = 0;
    ncs::ReminderService reminderService(database, preferences, favorites,
                                         [&clockMs] { return clockMs; });
    const auto checkReminder = [&](qint64 id, const QString &time) {
        clockMs += 5000;
        return reminderService.check(id, time);
    };

    const auto defaults = preferenceService.getPreference(userA);
    if (!check(defaults.success && defaults.value.homeRadiusKm == 3.0
               && defaults.value.reminderStartTime == QStringLiteral("08:00")
               && defaults.value.dndEndTime == QStringLiteral("07:00")
               && defaults.value.preferredChargerTypes.isEmpty(), "default preference")) return 3;

    ncs::UserPreference invalid = defaults.value;
    invalid.homeLatitudeSet = true;
    invalid.homeLongitudeSet = false;
    if (!check(!preferenceService.updatePreference(invalid).success, "paired home coordinates")) return 4;
    invalid = defaults.value;
    invalid.hasHomeLocation = true;
    invalid.homeLatitude = 91.0;
    invalid.homeLongitude = 116.0;
    if (!check(!preferenceService.updatePreference(invalid).success, "latitude validation")) return 5;
    invalid = defaults.value;
    invalid.hasHomeLocation = true;
    invalid.homeLatitude = 39.0;
    invalid.homeLongitude = 181.0;
    if (!check(!preferenceService.updatePreference(invalid).success, "longitude validation")) return 6;
    invalid = defaults.value;
    invalid.hasHomeLocation = true;
    invalid.homeLatitudeSet = true;
    invalid.homeLongitudeSet = true;
    invalid.homeLatitude = std::numeric_limits<double>::quiet_NaN();
    invalid.homeLongitude = 116.0;
    if (!check(!preferenceService.updatePreference(invalid).success, "NaN latitude validation")) return 7;
    invalid.homeLatitude = 39.0;
    invalid.homeLongitude = std::numeric_limits<double>::infinity();
    if (!check(!preferenceService.updatePreference(invalid).success, "infinite longitude validation")) return 8;
    invalid = defaults.value;
    invalid.hasHomeLocation = false;
    invalid.homeLatitudeSet = true;
    invalid.homeLongitudeSet = true;
    invalid.homeLatitude = 39.0;
    invalid.homeLongitude = 116.0;
    if (!check(!preferenceService.updatePreference(invalid).success,
               "home state and coordinates must agree")) return 9;
    invalid = defaults.value;
    invalid.homeRadiusKm = 0.0;
    if (!check(!preferenceService.updatePreference(invalid).success, "radius validation")) return 10;
    invalid = defaults.value;
    invalid.minIdleChargers = 0;
    if (!check(!preferenceService.updatePreference(invalid).success, "idle threshold validation")) return 11;
    invalid = defaults.value;
    invalid.preferredChargerTypes = {9};
    if (!check(!preferenceService.updatePreference(invalid).success, "charger type validation")) return 12;
    invalid = defaults.value;
    invalid.reminderStartTime = QStringLiteral("8:00");
    if (!check(!preferenceService.updatePreference(invalid).success, "time format validation")) return 13;
    invalid = defaults.value;
    invalid.reminderStartTime = QStringLiteral("08:10");
    invalid.reminderEndTime = QStringLiteral("08:20");
    invalid.dndStartTime = QStringLiteral("07:00");
    invalid.dndEndTime = QStringLiteral("09:00");
    if (!check(!preferenceService.updatePreference(invalid).success, "empty effective window")) return 14;
    invalid = defaults.value;
    invalid.reminderStartTime = QStringLiteral("08:00");
    invalid.reminderEndTime = QStringLiteral("12:00");
    invalid.dndStartTime = QStringLiteral("10:00");
    invalid.dndEndTime = QStringLiteral("11:00");
    if (!check(preferenceService.updatePreference(invalid).success, "partial overlap allowed")) return 15;

    if (!check(ncs::isTimeInWindow(QStringLiteral("23:00"), QStringLiteral("22:00"),
                                   QStringLiteral("07:00"))
               && ncs::isTimeInWindow(QStringLiteral("06:00"), QStringLiteral("22:00"),
                                       QStringLiteral("07:00"))
               && !ncs::isTimeInWindow(QStringLiteral("12:00"), QStringLiteral("22:00"),
                                        QStringLiteral("07:00")), "cross-midnight window")) return 16;

    ncs::UserPreference preference = defaults.value;
    preference.homeLatitudeSet = true;
    preference.homeLongitudeSet = true;
    preference.hasHomeLocation = true;
    preference.homeLatitude = 39.9623;
    preference.homeLongitude = 116.3220;
    preference.homeRadiusKm = 0.1;
    preference.preferredChargerTypes = {1, 1};
    preference.minIdleChargers = 4;
    preference.reminderStartTime = QStringLiteral("08:00");
    preference.reminderEndTime = QStringLiteral("22:00");
    preference.dndStartTime = QStringLiteral("22:00");
    preference.dndEndTime = QStringLiteral("07:00");
    if (!check(preferenceService.updatePreference(preference).success, "valid preference update")) return 17;
    const auto normalized = preferenceService.getPreference(userA);
    if (!check(normalized.success && normalized.value.preferredChargerTypes == QVector<int>{1},
               "charger type deduplication")) return 18;

    const auto inside = checkReminder(userA, QStringLiteral("12:00"));
    if (!check(inside.success && inside.value.size() == 1
               && inside.value.first().stationId == 1
               && inside.value.first().idleMatchedChargers == 4,
               "home radius and matched idle count")) return 19;
    const auto tooFrequent = reminderService.check(userA, QStringLiteral("12:00"));
    if (!check(!tooFrequent.success && tooFrequent.code == ncs::BusinessErrorCode::TooFrequent,
               "reminder rate limit")) return 20;
    if (!check(checkReminder(userA, QStringLiteral("23:00")).value.isEmpty(),
               "reminder window")) return 21;
    preference.reminderStartTime = QStringLiteral("00:00");
    preference.reminderEndTime = QStringLiteral("23:00");
    preference.dndStartTime = QStringLiteral("22:00");
    preference.dndEndTime = QStringLiteral("07:00");
    if (!check(preferenceService.updatePreference(preference).success
               && checkReminder(userA, QStringLiteral("23:00")).value.isEmpty(),
               "DND overrides reminder window")) return 22;
    preference.reminderStartTime = QStringLiteral("08:00");
    preference.reminderEndTime = QStringLiteral("22:00");
    if (!check(preferenceService.updatePreference(preference).success, "restore reminder window")) return 23;

    if (!check(preferenceService.favoriteStationAdd(userA, 2).success
               && !preferenceService.favoriteStationAdd(userA, 2).value,
               "idempotent favorite add")) return 24;
    const auto outsideFavorite = checkReminder(userA, QStringLiteral("12:00"));
    if (!check(outsideFavorite.success && outsideFavorite.value.size() == 2,
               "favorite OR home radius")) return 25;
    if (!check(preferenceService.favoriteStationList(userB).value.isEmpty(),
               "favorite user isolation")) return 26;
    if (!check(preferenceService.favoriteStationAdd(userA, 9999).code
                   == ncs::BusinessErrorCode::StationNotFound,
               "favorite station existence")) return 27;
    if (!check(preferenceService.favoriteStationRemove(userA, 2).success
               && !preferenceService.favoriteStationRemove(userA, 2).value,
               "idempotent favorite remove")) return 28;

    preference.hasHomeLocation = false;
    preference.homeLatitudeSet = false;
    preference.homeLongitudeSet = false;
    if (!check(preferenceService.updatePreference(preference).success
               && checkReminder(userA, QStringLiteral("12:00")).value.isEmpty(),
               "no home and no favorites")) return 29;

    preference.homeLatitudeSet = true;
    preference.homeLongitudeSet = true;
    preference.hasHomeLocation = true;
    preference.homeLatitude = 0.0;
    preference.homeLongitude = 0.0;
    preference.homeRadiusKm = 1.0;
    preference.preferredChargerTypes.clear();
    preference.minIdleChargers = 1;
    if (!check(preferenceService.updatePreference(preference).success, "far home update")) return 30;
    if (!check(checkReminder(userA, QStringLiteral("12:00")).value.isEmpty(),
               "nonfavorite outside radius")) return 31;
    if (!check(preferenceService.favoriteStationAdd(userA, 2).success
               && checkReminder(userA, QStringLiteral("12:00")).value.size() == 1,
               "favorite outside radius")) return 32;

    preference.hasHomeLocation = false;
    preference.homeLatitudeSet = false;
    preference.homeLongitudeSet = false;
    if (!check(preferenceService.updatePreference(preference).success, "unset home")) return 36;
    preference.preferredChargerTypes = {0};
    preference.minIdleChargers = 4;
    if (!check(preferenceService.updatePreference(preference).success
               && checkReminder(userA, QStringLiteral("12:00")).value.isEmpty(),
               "slow type threshold")) return 33;
    preference.preferredChargerTypes.clear();
    preference.minIdleChargers = 1;
    if (!check(preferenceService.updatePreference(preference).success
               && checkReminder(userA, QStringLiteral("12:00")).value.size() == 1,
               "empty types match all")) return 34;
    preference.enabled = false;
    if (!check(preferenceService.updatePreference(preference).success
               && checkReminder(userA, QStringLiteral("12:00")).value.isEmpty(),
               "disabled reminders")) return 35;
    return 0;
}
