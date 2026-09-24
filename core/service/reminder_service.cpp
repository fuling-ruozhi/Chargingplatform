#include "reminder_service.h"

#include "database/database_manager.h"
#include "model/charger.h"
#include "repository/favorite_station_repository.h"
#include "repository/user_preference_repository.h"
#include "util/geo_distance.h"
#include "util/time_window.h"

#include <QHash>
#include <QDateTime>
#include <QMutexLocker>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>

#include <algorithm>

namespace ncs {
namespace {

struct StationState
{
    qint64 id = 0;
    QString name;
    double longitude = 0.0;
    double latitude = 0.0;
    QVector<int> idleTypes;
};

}

ReminderService::ReminderService(DatabaseManager &database,
                                 UserPreferenceRepository &preferences,
                                 FavoriteStationRepository &favorites,
                                 std::function<qint64()> clock)
    : database_(database), preferences_(preferences), favorites_(favorites),
      clock_(std::move(clock))
{
    if (!clock_) clock_ = [] { return QDateTime::currentMSecsSinceEpoch(); };
}

ServiceResult<QVector<ReminderMatch>> ReminderService::check(qint64 userId,
                                                             const QString &currentTime) const
{
    const qint64 nowMs = clock_();
    {
        QMutexLocker locker(&rateLimitMutex_);
        const auto previous = lastCheckMs_.constFind(userId);
        if (previous != lastCheckMs_.cend() && nowMs - previous.value() < 5000) {
            return ServiceResult<QVector<ReminderMatch>>::fail(
                BusinessErrorCode::TooFrequent,
                QStringLiteral("提醒检查过于频繁，请稍后再试"));
        }
        lastCheckMs_.insert(userId, nowMs);
    }

    UserPreference preference;
    bool found = false;
    QString error;
    if (!preferences_.getByUserId(userId, &preference, &found, &error)) {
        return ServiceResult<QVector<ReminderMatch>>::fail(BusinessErrorCode::DatabaseError, error);
    }
    if (!found) {
        preference.userId = userId;
    } else if (!preferences_.preferredTypes(userId, &preference.preferredChargerTypes, &error)) {
        return ServiceResult<QVector<ReminderMatch>>::fail(BusinessErrorCode::DatabaseError, error);
    }
    if (!preference.enabled || !isTimeInWindow(currentTime, preference.reminderStartTime,
                                                preference.reminderEndTime)
        || isTimeInWindow(currentTime, preference.dndStartTime, preference.dndEndTime)) {
        return ServiceResult<QVector<ReminderMatch>>::ok({});
    }

    QVector<qint64> favoriteIds;
    if (!favorites_.listByUser(userId, &favoriteIds, &error)) {
        return ServiceResult<QVector<ReminderMatch>>::fail(BusinessErrorCode::DatabaseError, error);
    }
    const QSet<qint64> favoriteSet(favoriteIds.cbegin(), favoriteIds.cend());
    if (!preference.hasHomeLocation && favoriteSet.isEmpty()) {
        return ServiceResult<QVector<ReminderMatch>>::ok({});
    }

    QString stationFilter = QStringLiteral(
        "s.id IN (SELECT station_id FROM user_favorite_station WHERE user_id=:user_id)");
    double minLatitude = 0.0;
    double maxLatitude = 0.0;
    double minLongitude = 0.0;
    double maxLongitude = 0.0;
    bool filterLongitude = false;
    if (preference.hasHomeLocation) {
        constexpr double kmPerLatitudeDegree = 111.0;
        const double latitudeDelta = preference.homeRadiusKm / kmPerLatitudeDegree;
        minLatitude = std::max(-90.0, preference.homeLatitude - latitudeDelta);
        maxLatitude = std::min(90.0, preference.homeLatitude + latitudeDelta);

        const double latitudeRadians = qDegreesToRadians(preference.homeLatitude);
        const double cosine = qCos(latitudeRadians);
        if (qAbs(cosine) > 1e-9) {
            const double longitudeDelta = preference.homeRadiusKm
                / (kmPerLatitudeDegree * qAbs(cosine));
            if (longitudeDelta < 180.0) {
                minLongitude = std::max(-180.0, preference.homeLongitude - longitudeDelta);
                maxLongitude = std::min(180.0, preference.homeLongitude + longitudeDelta);
                filterLongitude = true;
            }
        }

        QString homeFilter = QStringLiteral(
            "s.latitude BETWEEN :min_latitude AND :max_latitude");
        if (filterLongitude) {
            homeFilter += QStringLiteral(
                " AND s.longitude BETWEEN :min_longitude AND :max_longitude");
        }
        stationFilter = QStringLiteral("(%1) OR (%2)").arg(stationFilter, homeFilter);
    }

    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT s.id,s.name,s.longitude,s.latitude,c.type,c.status "
            "FROM station s LEFT JOIN charger c ON c.station_id=s.id "
            "WHERE %1 ORDER BY s.id,c.id").arg(stationFilter))) {
        return ServiceResult<QVector<ReminderMatch>>::fail(
            BusinessErrorCode::DatabaseError, query.lastError().text());
    }
    query.bindValue(QStringLiteral(":user_id"), userId);
    if (preference.hasHomeLocation) {
        query.bindValue(QStringLiteral(":min_latitude"), minLatitude);
        query.bindValue(QStringLiteral(":max_latitude"), maxLatitude);
        if (filterLongitude) {
            query.bindValue(QStringLiteral(":min_longitude"), minLongitude);
            query.bindValue(QStringLiteral(":max_longitude"), maxLongitude);
        }
    }
    if (!query.exec()) {
        return ServiceResult<QVector<ReminderMatch>>::fail(
            BusinessErrorCode::DatabaseError, query.lastError().text());
    }
    QHash<qint64, StationState> states;
    while (query.next()) {
        const qint64 stationId = query.value(0).toLongLong();
        StationState &state = states[stationId];
        state.id = stationId;
        state.name = query.value(1).toString();
        state.longitude = query.value(2).toDouble();
        state.latitude = query.value(3).toDouble();
        if (!query.value(4).isNull()
            && query.value(5).toInt() == static_cast<int>(ChargerStatus::Idle)) {
            state.idleTypes.append(query.value(4).toInt());
        }
    }

    QSet<int> preferred(preference.preferredChargerTypes.cbegin(),
                       preference.preferredChargerTypes.cend());
    QVector<ReminderMatch> matches;
    for (const StationState &state : states) {
        const bool favorite = favoriteSet.contains(state.id);
        const double distance = preference.hasHomeLocation
            ? GeoDistance::haversineKm(preference.homeLongitude, preference.homeLatitude,
                                       state.longitude, state.latitude) : 0.0;
        const bool inside = preference.hasHomeLocation && distance <= preference.homeRadiusKm;
        if (!favorite && !inside) continue;

        QSet<int> matched;
        int idleCount = 0;
        for (const int type : state.idleTypes) {
            if (preferred.isEmpty() || preferred.contains(type)) {
                ++idleCount;
                matched.insert(type);
            }
        }
        if (idleCount < preference.minIdleChargers) continue;

        ReminderMatch match;
        match.stationId = state.id;
        match.stationName = state.name;
        match.hasDistance = preference.hasHomeLocation;
        match.distanceKm = distance;
        match.matchedTypes = matched.values().toVector();
        std::sort(match.matchedTypes.begin(), match.matchedTypes.end());
        match.idleMatchedChargers = idleCount;
        match.favorite = favorite;
        match.insideHomeRadius = inside;
        match.reason = QStringLiteral("达到最低空闲数");
        matches.append(match);
    }
    std::sort(matches.begin(), matches.end(), [](const ReminderMatch &left,
                                                 const ReminderMatch &right) {
        if (left.hasDistance && right.hasDistance && left.distanceKm != right.distanceKm) {
            return left.distanceKm < right.distanceKm;
        }
        return left.stationId < right.stationId;
    });
    return ServiceResult<QVector<ReminderMatch>>::ok(matches);
}

}
