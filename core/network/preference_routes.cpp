#include "preference_routes.h"

#include "business_route_json.h"
#include "json_protocol.h"
#include "request_router.h"
#include "service/reminder_service.h"
#include "service/session_manager.h"
#include "service/user_preference_service.h"

#include <QJsonArray>
#include <QTime>

namespace ncs {
namespace {

bool resolveUser(const JsonRequest &request, SessionManager &sessions,
                 qint64 *userId, JsonResponse *failure)
{
    BusinessErrorCode code = BusinessErrorCode::AuthRequired;
    QString message;
    if (sessions.resolveUser(request.data.value(QStringLiteral("session_token")).toString(),
                             userId, &code, &message)) return true;
    *failure = JsonProtocol::failure(request.requestId, static_cast<int>(code), message);
    return false;
}

}

void PreferenceRoutes::registerAll(RequestRouter &router, SessionManager &sessions,
                                    UserPreferenceService &preferences,
                                    ReminderService &reminders)
{
    router.registerHandler(QStringLiteral("preference.get"), [&](const JsonRequest &request) {
        qint64 userId = 0; JsonResponse failure;
        if (!resolveUser(request, sessions, &userId, &failure)) return failure;
        const auto result = preferences.getPreference(userId);
        return result.success
            ? JsonProtocol::success(request.requestId, userPreferenceJson(result.value))
            : JsonProtocol::failure(request.requestId, static_cast<int>(result.code), result.message);
    });
    router.registerHandler(QStringLiteral("preference.update"), [&](const JsonRequest &request) {
        qint64 userId = 0; JsonResponse failure;
        if (!resolveUser(request, sessions, &userId, &failure)) return failure;
        const QJsonValue latitude = request.data.value(QStringLiteral("home_latitude"));
        const QJsonValue longitude = request.data.value(QStringLiteral("home_longitude"));
        if (latitude.isNull() != longitude.isNull()
            || (!latitude.isNull() && (!latitude.isDouble() || !longitude.isDouble()))) {
            return JsonProtocol::failure(request.requestId, static_cast<int>(BusinessErrorCode::InvalidArgument),
                                         QStringLiteral("家庭位置必须同时提供经纬度"));
        }
        UserPreference preference;
        preference.userId = userId;
        preference.hasHomeLocation = !latitude.isNull();
        preference.homeLatitudeSet = !latitude.isNull();
        preference.homeLongitudeSet = !longitude.isNull();
        if (preference.hasHomeLocation) {
            preference.homeLatitude = latitude.toDouble();
            preference.homeLongitude = longitude.toDouble();
        }
        preference.homeRadiusKm = request.data.value(QStringLiteral("home_radius_km")).toDouble(3.0);
        preference.reminderStartTime = request.data.value(QStringLiteral("reminder_start_time")).toString();
        preference.reminderEndTime = request.data.value(QStringLiteral("reminder_end_time")).toString();
        preference.minIdleChargers = request.data.value(QStringLiteral("min_idle_chargers")).toInt(1);
        preference.dndStartTime = request.data.value(QStringLiteral("dnd_start_time")).toString();
        preference.dndEndTime = request.data.value(QStringLiteral("dnd_end_time")).toString();
        preference.enabled = request.data.value(QStringLiteral("enabled")).toBool(true);
        for (const QJsonValue &value : request.data.value(QStringLiteral("preferred_charger_types")).toArray())
            preference.preferredChargerTypes.append(value.toInt());
        const auto result = preferences.updatePreference(preference);
        return result.success
            ? JsonProtocol::success(request.requestId, userPreferenceJson(result.value))
            : JsonProtocol::failure(request.requestId, static_cast<int>(result.code), result.message);
    });
    router.registerHandler(QStringLiteral("favorite_station.list"), [&](const JsonRequest &request) {
        qint64 userId = 0; JsonResponse failure;
        if (!resolveUser(request, sessions, &userId, &failure)) return failure;
        const auto result = preferences.favoriteStationList(userId);
        if (!result.success)
            return JsonProtocol::failure(request.requestId, static_cast<int>(result.code), result.message);
        QJsonArray stations;
        for (const qint64 stationId : result.value) stations.append(stationId);
        return JsonProtocol::success(request.requestId, {{QStringLiteral("station_ids"), stations}});
    });
    const auto favoriteAction = [&](const QString &route, bool adding) {
        router.registerHandler(route, [&, adding](const JsonRequest &request) {
            qint64 userId = 0; JsonResponse failure;
            if (!resolveUser(request, sessions, &userId, &failure)) return failure;
            const qint64 stationId = request.data.value(QStringLiteral("station_id")).toInteger();
            const auto result = adding ? preferences.favoriteStationAdd(userId, stationId)
                                        : preferences.favoriteStationRemove(userId, stationId);
            return result.success
                ? JsonProtocol::success(request.requestId,
                    {{QStringLiteral("station_id"), stationId},
                     {adding ? QStringLiteral("added") : QStringLiteral("removed"), result.value}})
                : JsonProtocol::failure(request.requestId, static_cast<int>(result.code), result.message);
        });
    };
    favoriteAction(QStringLiteral("favorite_station.add"), true);
    favoriteAction(QStringLiteral("favorite_station.remove"), false);
    router.registerHandler(QStringLiteral("reminder.check"), [&](const JsonRequest &request) {
        qint64 userId = 0; JsonResponse failure;
        if (!resolveUser(request, sessions, &userId, &failure)) return failure;
        const auto result = reminders.check(userId,
            QTime::currentTime().toString(QStringLiteral("HH:mm")));
        if (!result.success)
            return JsonProtocol::failure(request.requestId, static_cast<int>(result.code), result.message);
        QJsonArray alerts;
        for (const ReminderMatch &match : result.value) alerts.append(reminderMatchJson(match));
        return JsonProtocol::success(request.requestId, {{QStringLiteral("alerts"), alerts}});
    });
}

}
