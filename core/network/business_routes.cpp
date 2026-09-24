#include "business_routes.h"
#include "business_route_json.h"
#include "business_routes_helpers.h"
#include "json_protocol.h"
#include "review_json.h"
#include "request_router.h"
#include "preference_routes.h"
#include "service/charge_service.h"
#include "service/review_service.h"
#include "service/session_manager.h"
#include "service/charge_forecast_service.h"
#include "model/charge_forecast.h"
#include "service/station_service.h"
#include "service/user_service.h"

#include <QJsonArray>
#include <QtGlobal>

namespace ncs {
namespace {

VehicleProfile vehicleProfileFromRequest(const QJsonObject &data, qint64 userId)
{
    VehicleProfile profile;
    profile.userId = userId;
    profile.batteryCapacityKwh = data.value(QStringLiteral("battery_capacity_kwh")).toDouble();
    profile.targetSoc = data.value(QStringLiteral("target_soc")).toDouble();
    profile.minBalanceReserve = data.value(QStringLiteral("min_balance_reserve")).toDouble();
    profile.usualLeaveTime = data.value(QStringLiteral("usual_leave_time")).toString();
    profile.chargeMode = data.value(QStringLiteral("charge_mode")).toInt(0);
    return profile;
}

QJsonObject stationJson(const Station &station)
{
    return {{QStringLiteral("id"), station.id},
            {QStringLiteral("name"), station.name},
            {QStringLiteral("address"), station.address},
            {QStringLiteral("price"), station.price},
            {QStringLiteral("total_slots"), station.totalSlots},
            {QStringLiteral("longitude"), station.longitude},
            {QStringLiteral("latitude"), station.latitude},
            {QStringLiteral("idle_slots"), station.idleSlots},
            {QStringLiteral("distance_km"), station.distanceKm},
            {QStringLiteral("charger_count"), station.chargerCount},
            {QStringLiteral("rating_summary"), QJsonObject{
                {QStringLiteral("review_count"), station.reviewCount},
                {QStringLiteral("average_score"), station.averageScore},
                {QStringLiteral("queue_score"), station.queueScore},
                {QStringLiteral("environment_score"), station.environmentScore},
                {QStringLiteral("equipment_score"), station.equipmentScore},
                {QStringLiteral("parking_score"), station.parkingScore}}},
            {QStringLiteral("recommend_score"), station.recommendScore}};
}

QJsonObject chargerJson(const Charger &charger)
{
    return {{QStringLiteral("id"), charger.id},
            {QStringLiteral("station_id"), charger.stationId},
            {QStringLiteral("code"), charger.code},
            {QStringLiteral("status"), static_cast<int>(charger.status)},
            {QStringLiteral("active_order_status"), charger.activeOrderStatus},
            {QStringLiteral("type"), charger.type},
            {QStringLiteral("power_kw"), charger.powerKw},
            {QStringLiteral("total_count"), charger.totalCount},
            {QStringLiteral("total_minutes"), charger.totalMinutes}};
}

QJsonObject chargingOrderJson(const ChargingRecord &record)
{
    return {{QStringLiteral("id"), record.id},
            {QStringLiteral("order_id"), record.id},
            {QStringLiteral("order_no"), record.orderNo},
            {QStringLiteral("user_id"), record.userId},
            {QStringLiteral("charger_id"), record.chargerId},
            {QStringLiteral("start_time"), record.startTime},
            {QStringLiteral("end_time"), record.endTime},
            {QStringLiteral("energy"), record.energy},
            {QStringLiteral("amount"), record.cost},
            {QStringLiteral("duration_seconds"), record.durationSeconds},
            {QStringLiteral("status"), static_cast<int>(record.status)},
            {QStringLiteral("reserved_at"), record.reservedAt},
            {QStringLiteral("expire_at"), record.expireAt},
            {QStringLiteral("station_id"), record.stationId},
            {QStringLiteral("station_name"), record.stationName},
            {QStringLiteral("charger_code"), record.chargerCode},
            {QStringLiteral("price_per_kwh"), record.price},
            {QStringLiteral("power_kw"), record.powerKw},
            {QStringLiteral("time_scale"), record.timeScale},
            {QStringLiteral("initial_soc"), record.initialSoc},
            {QStringLiteral("final_soc"), record.finalSoc},
            {QStringLiteral("debt_amount"), record.debtAmount},
            {QStringLiteral("balance_after"), record.balanceAfter}};
}

JsonResponse userResponse(const JsonRequest &request, const ServiceResult<User> &result)
{
    return result.success
        ? JsonProtocol::success(request.requestId, userJson(result.value))
        : JsonProtocol::failure(request.requestId, static_cast<int>(result.code), result.message);
}

}

void BusinessRoutes::registerAll(RequestRouter &router, UserService &userService,
                                 StationService &stationService, ChargeService &chargeService,
                                 SessionManager &sessionManager, ReviewService &reviewService,
                                 UserPreferenceService &preferenceService,
                                 ReminderService &reminderService,
                                 ChargeForecastService &forecastService)
{
    router.registerHandler(QStringLiteral("user.otp.request"), [&](const JsonRequest &request) {
        const auto result = userService.requestOtp(
            request.data.value(QStringLiteral("phone")).toString());
        if (!result.success) {
            return JsonProtocol::failure(request.requestId, static_cast<int>(result.code),
                                         result.message);
        }
        QJsonObject data{{QStringLiteral("cooldown_seconds"), result.value.cooldownSeconds},
                         {QStringLiteral("expires_seconds"), result.value.expiresSeconds}};
        if (shouldEchoOtpForDemo(request)) {
            data.insert(QStringLiteral("display_code"), result.value.displayCode);
        }
        return JsonProtocol::success(request.requestId, data);
    });
    router.registerHandler(QStringLiteral("user.login"), [&](const JsonRequest &request) {
        const auto result = userService.loginWithOtp(
            request.data.value(QStringLiteral("phone")).toString(),
            request.data.value(QStringLiteral("code")).toString());
        if (!result.success) {
            return JsonProtocol::failure(request.requestId, static_cast<int>(result.code),
                                         result.message);
        }
        return JsonProtocol::success(request.requestId,
            {{QStringLiteral("session_token"), sessionManager.issueUserToken(result.value.id)},
             {QStringLiteral("user"), userJson(result.value)}});
    });
    router.registerHandler(QStringLiteral("user.profile.get"), [&](const JsonRequest &request) {
        qint64 userId = 0; JsonResponse failure;
        if (!resolveUser(request, sessionManager, &userId, &failure)) return failure;
        return userResponse(request, userService.profile(userId));
    });
    registerForecastRoute(router, forecastService, sessionManager);
    router.registerHandler(QStringLiteral("user.profile.nickname.update"),
                           [&](const JsonRequest &request) {
        qint64 userId = 0; JsonResponse failure;
        if (!resolveUser(request, sessionManager, &userId, &failure)) return failure;
        return userResponse(request, userService.updateNickname(
            userId, request.data.value(QStringLiteral("nickname")).toString()));
    });
    router.registerHandler(QStringLiteral("user.profile.avatar.update"),
                           [&](const JsonRequest &request) {
        qint64 userId = 0; JsonResponse failure;
        if (!resolveUser(request, sessionManager, &userId, &failure)) return failure;
            return userResponse(request, userService.updateAvatar(
            userId, request.data.value(QStringLiteral("avatar_path")).toString()));
    });
    PreferenceRoutes::registerAll(router, sessionManager, preferenceService, reminderService);
    router.registerHandler(QStringLiteral("user.vehicle.profile.get"),
                           [&](const JsonRequest &request) {
        qint64 userId = 0; JsonResponse failure;
        if (!resolveUser(request, sessionManager, &userId, &failure)) return failure;
        const auto result = userService.vehicleProfile(userId);
        return result.success
            ? JsonProtocol::success(request.requestId, vehicleProfileJson(result.value))
            : JsonProtocol::failure(request.requestId, static_cast<int>(result.code), result.message);
    });
    router.registerHandler(QStringLiteral("user.vehicle.profile.update"),
                           [&](const JsonRequest &request) {
        qint64 userId = 0; JsonResponse failure;
        if (!resolveUser(request, sessionManager, &userId, &failure)) return failure;
        const auto result = userService.updateVehicleProfile(userId, vehicleProfileFromRequest(request.data, userId));
        return result.success
            ? JsonProtocol::success(request.requestId, vehicleProfileJson(result.value))
            : JsonProtocol::failure(request.requestId, static_cast<int>(result.code), result.message);
    });
    router.registerHandler(QStringLiteral("user.recharge"), [&](const JsonRequest &request) {
        qint64 userId = 0; JsonResponse failure;
        if (!resolveUser(request, sessionManager, &userId, &failure)) return failure;
        const auto result = userService.recharge(
            userId, request.data.value(QStringLiteral("amount")).toDouble());
        return result.success
            ? JsonProtocol::success(request.requestId,
                {{QStringLiteral("log_id"), result.value.logId},
                 {QStringLiteral("amount"), result.value.amount},
                 {QStringLiteral("balance_before"), result.value.balanceBefore},
                 {QStringLiteral("balance_after"), result.value.balanceAfter}})
            : JsonProtocol::failure(request.requestId, static_cast<int>(result.code),
                                    result.message);
    });
    router.registerHandler(QStringLiteral("user.logout"), [&](const JsonRequest &request) {
        qint64 userId = 0; JsonResponse failure;
        if (!resolveUser(request, sessionManager, &userId, &failure)) return failure;
        Q_UNUSED(userId)
        sessionManager.invalidateUser(
            request.data.value(QStringLiteral("session_token")).toString());
        return JsonProtocol::success(request.requestId);
    });
    router.registerHandler(QStringLiteral("legacy.user.register"), [&](const JsonRequest &request) {
        const auto result = userService.registerUser(
            request.data.value(QStringLiteral("username")).toString(),
            request.data.value(QStringLiteral("password")).toString());
        if (!result.success) {
            return JsonProtocol::failure(request.requestId, static_cast<int>(result.code),
                                         result.message);
        }
        QJsonObject data = legacyUserJson(result.value);
        data.insert(QStringLiteral("session_token"),
                    sessionManager.issueUserToken(result.value.id));
        data.insert(QStringLiteral("user"), legacyUserJson(result.value));
        return JsonProtocol::success(request.requestId, data);
    });
    router.registerHandler(QStringLiteral("legacy.user.login"), [&](const JsonRequest &request) {
        const auto result = userService.login(
            request.data.value(QStringLiteral("username")).toString(),
            request.data.value(QStringLiteral("password")).toString());
        if (!result.success) {
            return JsonProtocol::failure(request.requestId, static_cast<int>(result.code),
                                         result.message);
        }
        QJsonObject data = legacyUserJson(result.value);
        data.insert(QStringLiteral("session_token"),
                    sessionManager.issueUserToken(result.value.id));
        data.insert(QStringLiteral("user"), legacyUserJson(result.value));
        return JsonProtocol::success(request.requestId, data);
    });
    router.registerHandler(QStringLiteral("station.list"), [&](const JsonRequest &request) {
        const auto cleanup = chargeService.cleanupExpired();
        if (!cleanup.success) {
            return JsonProtocol::failure(request.requestId, static_cast<int>(cleanup.code),
                                         cleanup.message);
        }
        const bool hasOrigin = request.data.value(QStringLiteral("longitude")).isDouble()
            && request.data.value(QStringLiteral("latitude")).isDouble();
        const auto result = stationService.list(
            request.data.value(QStringLiteral("longitude")).toDouble(),
            request.data.value(QStringLiteral("latitude")).toDouble(), hasOrigin);
        if (!result.success) {
            return JsonProtocol::failure(request.requestId, static_cast<int>(result.code),
                                         result.message);
        }
        QJsonArray stations;
        for (const Station &station : result.value) {
            stations.append(stationJson(station));
        }
        return JsonProtocol::success(request.requestId,
                                     {{QStringLiteral("stations"), stations}});
    });
    router.registerHandler(QStringLiteral("station.detail"), [&](const JsonRequest &request) {
        const auto cleanup = chargeService.cleanupExpired();
        if (!cleanup.success) {
            return JsonProtocol::failure(request.requestId, static_cast<int>(cleanup.code),
                                         cleanup.message);
        }
        const bool hasOrigin = request.data.value(QStringLiteral("origin_longitude")).isDouble()
            && request.data.value(QStringLiteral("origin_latitude")).isDouble();
        const auto result = stationService.detail(
            request.data.value(QStringLiteral("station_id")).toInteger(),
            request.data.value(QStringLiteral("origin_longitude")).toDouble(),
            request.data.value(QStringLiteral("origin_latitude")).toDouble(), hasOrigin);
        if (!result.success) {
            return JsonProtocol::failure(request.requestId, static_cast<int>(result.code),
                                         result.message);
        }
        QJsonArray chargers;
        for (const Charger &charger : result.value.chargers) chargers.append(chargerJson(charger));
        QJsonObject data = stationJson(result.value.station);
        data.insert(QStringLiteral("chargers"), chargers);
        data.insert(QStringLiteral("rating_summary"), ratingSummaryJson(
            result.value.ratingSummary));
        return JsonProtocol::success(request.requestId, data);
    });
    router.registerHandler(QStringLiteral("review.submit"), [&](const JsonRequest &request) {
        qint64 userId = 0;
        JsonResponse failure;
        if (!resolveUser(request, sessionManager, &userId, &failure)) return failure;
        const auto result = reviewService.submitReview(
            userId, request.data.value(QStringLiteral("order_id")).toInteger(),
            request.data.value(QStringLiteral("environment_score")).toInt(),
            request.data.value(QStringLiteral("queue_score")).toInt(),
            request.data.value(QStringLiteral("equipment_score")).toInt(),
            request.data.value(QStringLiteral("parking_score")).toInt());
        return result.success
            ? JsonProtocol::success(request.requestId, reviewJson(result.value))
            : JsonProtocol::failure(request.requestId, static_cast<int>(result.code),
                                    result.message);
    });
    router.registerHandler(QStringLiteral("review.get"), [&](const JsonRequest &request) {
        qint64 userId = 0;
        JsonResponse failure;
        if (!resolveUser(request, sessionManager, &userId, &failure)) return failure;
        const auto result = reviewService.reviewForOrder(
            userId, request.data.value(QStringLiteral("order_id")).toInteger());
        if (!result.success) {
            return JsonProtocol::failure(request.requestId, static_cast<int>(result.code),
                                         result.message);
        }
        QJsonObject data{{QStringLiteral("has_review"), result.value.id > 0}};
        if (result.value.id > 0) {
            const QJsonObject review = reviewJson(result.value);
            for (auto it = review.constBegin(); it != review.constEnd(); ++it) {
                data.insert(it.key(), it.value());
            }
        }
        return JsonProtocol::success(request.requestId, data);
    });
    registerStationRecommendation(router, stationService, sessionManager);
    router.registerHandler(QStringLiteral("charge.reserve"), [&](const JsonRequest &request) {
        qint64 userId = 0; JsonResponse failure;
        if (!resolveUser(request, sessionManager, &userId, &failure)) return failure;
        const auto result = chargeService.reserve(
            userId,
            request.data.value(QStringLiteral("charger_id")).toInteger());
        return result.success
            ? JsonProtocol::success(request.requestId, chargingOrderJson(result.value))
            : JsonProtocol::failure(request.requestId, static_cast<int>(result.code), result.message);
    });
    router.registerHandler(QStringLiteral("charge.start"), [&](const JsonRequest &request) {
        qint64 userId = 0; JsonResponse failure;
        if (!resolveUser(request, sessionManager, &userId, &failure)) return failure;
        const qint64 orderId = request.data.value(QStringLiteral("order_id")).toInteger();
        const auto result = orderId > 0
            ? chargeService.startReserved(userId, orderId)
            : chargeService.start(
                userId, request.data.value(QStringLiteral("charger_id")).toInteger());
        return result.success
            ? JsonProtocol::success(request.requestId, chargingOrderJson(result.value))
            : JsonProtocol::failure(request.requestId, static_cast<int>(result.code), result.message);
    });
    router.registerHandler(QStringLiteral("charge.settle"), [&](const JsonRequest &request) {
        qint64 userId = 0; JsonResponse failure;
        if (!resolveUser(request, sessionManager, &userId, &failure)) return failure;
        const auto result = chargeService.stop(
            request.data.value(QStringLiteral("order_id")).toInteger(), userId);
        return result.success
            ? JsonProtocol::success(request.requestId, chargingOrderJson(result.value))
            : JsonProtocol::failure(request.requestId, static_cast<int>(result.code), result.message);
    });
    router.registerHandler(QStringLiteral("charge.cancel"), [&](const JsonRequest &request) {
        qint64 userId = 0; JsonResponse failure;
        if (!resolveUser(request, sessionManager, &userId, &failure)) return failure;
        const auto result = chargeService.cancel(
            userId, request.data.value(QStringLiteral("order_id")).toInteger());
        return result.success
            ? JsonProtocol::success(request.requestId, chargingOrderJson(result.value))
            : JsonProtocol::failure(request.requestId, static_cast<int>(result.code), result.message);
    });
    router.registerHandler(QStringLiteral("charge.active"), [&](const JsonRequest &request) {
        qint64 userId = 0; JsonResponse failure;
        if (!resolveUser(request, sessionManager, &userId, &failure)) return failure;
        const auto result = chargeService.active(userId);
        if (!result.success) {
            return JsonProtocol::failure(request.requestId, static_cast<int>(result.code),
                                         result.message);
        }
        QJsonObject data{{QStringLiteral("has_active"), result.value.id > 0}};
        if (result.value.id > 0) {
            const QJsonObject order = chargingOrderJson(result.value);
            for (auto it = order.constBegin(); it != order.constEnd(); ++it) {
                data.insert(it.key(), it.value());
            }
        }
        return JsonProtocol::success(request.requestId, data);
    });
    router.registerHandler(QStringLiteral("order.list"), [&](const JsonRequest &request) {
        qint64 userId = 0; JsonResponse failure;
        if (!resolveUser(request, sessionManager, &userId, &failure)) return failure;
        const auto result = chargeService.history(
            userId, request.data.value(QStringLiteral("page")).toInt(1),
            request.data.value(QStringLiteral("page_size")).toInt(20));
        if (!result.success) {
            return JsonProtocol::failure(request.requestId, static_cast<int>(result.code),
                                         result.message);
        }
        QJsonArray orders;
        for (const ChargingRecord &record : result.value) {
            orders.append(chargingOrderJson(record));
        }
        return JsonProtocol::success(request.requestId,
                                     {{QStringLiteral("orders"), orders}});
    });
    router.registerHandler(QStringLiteral("order.detail"), [&](const JsonRequest &request) {
        qint64 userId = 0; JsonResponse failure;
        if (!resolveUser(request, sessionManager, &userId, &failure)) return failure;
        const auto result = chargeService.orderDetail(
            userId, request.data.value(QStringLiteral("order_id")).toInteger());
        return result.success
            ? JsonProtocol::success(request.requestId, chargingOrderJson(result.value))
            : JsonProtocol::failure(request.requestId, static_cast<int>(result.code),
                                    result.message);
    });
}

}
