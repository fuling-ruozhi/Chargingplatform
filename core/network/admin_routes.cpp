#include "admin_routes.h"
#include "admin_route_json.h"
#include "json_protocol.h"
#include "model/revenue_stats.h"
#include "model/charger_status_summary.h"
#include "model/charger.h"
#include "model/station.h"
#include "model/user.h"
#include "model/charging_record.h"
#include "request_router.h"
#include "service/admin_service.h"
#include "service/session_manager.h"
#include "service/station_rating_insight.h"
#include "service/log_service.h"
#include "admin_route_validation.h"

#include <QJsonArray>
#include <QJsonValue>

#include <functional>

namespace ncs {
namespace {
bool resolveAdmin(const JsonRequest &request, SessionManager &sessions,
                  qint64 *adminId, JsonResponse *failure)
{
    BusinessErrorCode code = BusinessErrorCode::AuthRequired;
    QString message;
    if (sessions.resolveAdmin(
            request.data.value(QStringLiteral("admin_session_token")).toString(),
            adminId, &code, &message)) return true;
    *failure = JsonProtocol::failure(
        request.requestId, static_cast<int>(code), message);
    return false;
}

QJsonObject chargerJson(const Charger &charger)
{
    return {{QStringLiteral("id"), charger.id},
            {QStringLiteral("station_id"), charger.stationId},
            {QStringLiteral("station_name"), charger.stationName},
            {QStringLiteral("code"), charger.code},
            {QStringLiteral("status"), static_cast<int>(charger.status)},
            {QStringLiteral("active_order_status"), charger.activeOrderStatus},
            {QStringLiteral("type"), charger.type},
            {QStringLiteral("power_kw"), charger.powerKw},
            {QStringLiteral("total_count"), charger.totalCount},
            {QStringLiteral("total_minutes"), charger.totalMinutes}};
}
QJsonObject stationJson(const Station &station)
{
    const StationRatingSummary summary{station.reviewCount, station.averageScore,
                                       station.environmentScore, station.queueScore,
                                       station.equipmentScore, station.parkingScore};
    QJsonArray insights;
    for (const auto &insight : stationRatingInsights(summary))
        insights.append(QJsonObject{{QStringLiteral("type"), insight.type},
                                    {QStringLiteral("level"), insight.level},
                                    {QStringLiteral("message"), insight.message}});
    return {{QStringLiteral("id"), station.id}, {QStringLiteral("name"), station.name},
            {QStringLiteral("address"), station.address}, {QStringLiteral("longitude"), station.longitude},
            {QStringLiteral("latitude"), station.latitude}, {QStringLiteral("price"), station.price},
            {QStringLiteral("total_slots"), station.totalSlots}, {QStringLiteral("charger_count"), station.chargerCount},
            {QStringLiteral("idle_slots"), station.idleSlots},
            {QStringLiteral("rating_summary"), QJsonObject{
                {QStringLiteral("review_count"), station.reviewCount},
                {QStringLiteral("average_score"), station.averageScore},
                {QStringLiteral("environment_score"), station.environmentScore},
                {QStringLiteral("queue_score"), station.queueScore},
                {QStringLiteral("equipment_score"), station.equipmentScore},
                {QStringLiteral("parking_score"), station.parkingScore}}},
            {QStringLiteral("operation_insights"), insights}};
}
QJsonObject userJson(const User &user)
{
    return {{QStringLiteral("id"), user.id}, {QStringLiteral("username"), user.username},
            {QStringLiteral("phone"), user.phone}, {QStringLiteral("nickname"), user.nickname},
            {QStringLiteral("status"), user.status}, {QStringLiteral("created_at"), user.createdAt}};
}
QJsonObject orderJson(const ChargingRecord &record)
{
    return {{QStringLiteral("id"), record.id}, {QStringLiteral("order_no"), record.orderNo},
            {QStringLiteral("user_id"), record.userId}, {QStringLiteral("charger_id"), record.chargerId},
            {QStringLiteral("charger_code"), record.chargerCode}, {QStringLiteral("station_id"), record.stationId},
            {QStringLiteral("station_name"), record.stationName}, {QStringLiteral("start_time"), record.startTime},
            {QStringLiteral("end_time"), record.endTime}, {QStringLiteral("amount"), record.cost},
            {QStringLiteral("status"), static_cast<int>(record.status)}};
}
bool positiveId(const QJsonValue &value, qint64 *id)
{
    if (!value.isDouble() || value.toDouble() <= 0.0
        || value.toDouble() != static_cast<double>(value.toInteger())) return false;
    *id = value.toInteger();
    return true;
}
JsonResponse invalidParameter(AdminService &service, qint64 adminId,
                              const QString &route, const QString &parameter,
                              const QString &reason, const QString &requestId)
{
    if (service.logService()) service.logService()->invalidParameter(adminId, route, parameter, reason);
    return JsonProtocol::failure(requestId, static_cast<int>(BusinessErrorCode::InvalidArgument), QStringLiteral("请求参数无效"));
}
}
void AdminRoutes::registerAll(RequestRouter &router, AdminService &adminService,
                              SessionManager &sessionManager)
{
    router.registerHandler(QStringLiteral("admin.login"), [&](const JsonRequest &request) {
        QString parameter, reason;
        if (!validAdminLoginInput(request.data, &parameter, &reason))
            return invalidParameter(adminService, 0, QStringLiteral("admin.login"), parameter, reason, request.requestId);
        const QString username = request.data.value(QStringLiteral("username")).toString();
        const auto result = adminService.login(
            username, request.data.value(QStringLiteral("password")).toString());
        if (!result.success) {
            QJsonObject data;
            if (result.code == BusinessErrorCode::AdminLocked) {
                data.insert(QStringLiteral("retry_after_seconds"),
                            adminService.retryAfterSeconds(username));
            }
            return JsonProtocol::failure(request.requestId,
                static_cast<int>(result.code), result.message, data);
        }
        return JsonProtocol::success(request.requestId,
            {{QStringLiteral("admin_session_token"),
              sessionManager.issueAdminToken(result.value.id)},
             {QStringLiteral("admin"), QJsonObject{
                 {QStringLiteral("id"), result.value.id},
                 {QStringLiteral("username"), result.value.username},
                 {QStringLiteral("created_at"), result.value.createdAt}}}});
    });
    router.registerHandler(QStringLiteral("admin.logout"), [&](const JsonRequest &request) {
        qint64 adminId = 0;
        JsonResponse failure;
        if (!resolveAdmin(request, sessionManager, &adminId, &failure)) return failure;
        Q_UNUSED(adminId)
        sessionManager.invalidateAdmin(
            request.data.value(QStringLiteral("admin_session_token")).toString());
        if (adminService.logService()) adminService.logService()->operation(adminId, "AUTH", "LOGOUT", "ADMIN", QString::number(adminId), "{}", true);
        return JsonProtocol::success(request.requestId);
    });
    registerLogRoutes(router, adminService, sessionManager);
    router.registerHandler(QStringLiteral("admin.summary"), [&](const JsonRequest &request) {
        qint64 adminId = 0;
        JsonResponse failure;
        if (!resolveAdmin(request, sessionManager, &adminId, &failure)) return failure;
        const auto result = adminService.summary(adminId);
        return result.success
            ? JsonProtocol::success(request.requestId,
                {{QStringLiteral("database_path"), result.value.databasePath},
                 {QStringLiteral("online_chargers"), result.value.onlineChargers},
                 {QStringLiteral("total_chargers"), result.value.totalChargers}})
            : JsonProtocol::failure(request.requestId,
                static_cast<int>(result.code), result.message);
    });
    router.registerHandler(QStringLiteral("admin.revenue.summary"),
                           [&](const JsonRequest &request) {
        qint64 adminId = 0;
        JsonResponse failure;
        if (!resolveAdmin(request, sessionManager, &adminId, &failure)) return failure;
        const auto result = adminService.revenueSummary(adminId);
        return result.success
            ? JsonProtocol::success(request.requestId, revenueSummaryJson(result.value))
            : JsonProtocol::failure(request.requestId,
                static_cast<int>(result.code), result.message);
    });
    router.registerHandler(QStringLiteral("admin.revenue.trend"),
                           [&](const JsonRequest &request) {
        qint64 adminId = 0;
        JsonResponse failure;
        if (!resolveAdmin(request, sessionManager, &adminId, &failure)) return failure;
        const QJsonValue value = request.data.value(QStringLiteral("days"));
        const int days = value.isDouble() ? value.toInt(-1) : -1;
        if (!value.isDouble() || days != value.toDouble()) {
            return invalidParameter(adminService, adminId, QStringLiteral("admin.revenue.trend"), QStringLiteral("days"), QStringLiteral("expected integer"), request.requestId);
        }
        if (days != 7 && days != 30) return invalidParameter(adminService, adminId, QStringLiteral("admin.revenue.trend"), QStringLiteral("days"), QStringLiteral("expected 7 or 30"), request.requestId);
        const auto result = adminService.revenueTrend(adminId, days);
        if (!result.success) {
            return JsonProtocol::failure(request.requestId,
                static_cast<int>(result.code), result.message);
        }
        QJsonArray items;
        for (const RevenueDay &day : result.value.items) {
            items.append(QJsonObject{{QStringLiteral("date"), day.date.toString(Qt::ISODate)},
                                     {QStringLiteral("revenue"), day.revenue},
                                     {QStringLiteral("order_count"), day.orderCount}});
        }
        return JsonProtocol::success(request.requestId,
            {{QStringLiteral("days"), result.value.days},
             {QStringLiteral("items"), items}});
    });
    router.registerHandler(QStringLiteral("admin.revenue.recentOrders"),
                           [&](const JsonRequest &request) {
        qint64 adminId = 0;
        JsonResponse failure;
        if (!resolveAdmin(request, sessionManager, &adminId, &failure)) return failure;
        const auto result = adminService.recentOrders(adminId);
        if (!result.success) {
            return JsonProtocol::failure(request.requestId,
                static_cast<int>(result.code), result.message);
        }
        QJsonArray items;
        for (const RecentOrder &order : result.value) {
            items.append(QJsonObject{{QStringLiteral("id"), order.id},
                                     {QStringLiteral("charger_code"), order.chargerCode},
                                     {QStringLiteral("station_name"), order.stationName},
                                     {QStringLiteral("start_time"), order.startTime},
                                     {QStringLiteral("end_time"), order.endTime},
                                     {QStringLiteral("cost"), order.cost}});
        }
        return JsonProtocol::success(request.requestId,
                                     {{QStringLiteral("items"), items}});
    });
    router.registerHandler(QStringLiteral("admin.charger.statusSummary"),
                           [&](const JsonRequest &request) {
        qint64 adminId = 0;
        JsonResponse failure;
        if (!resolveAdmin(request, sessionManager, &adminId, &failure)) return failure;
        const auto result = adminService.chargerStatusSummary(adminId);
            return result.success
            ? JsonProtocol::success(request.requestId, chargerStatusJson(result.value))
            : JsonProtocol::failure(request.requestId,
                static_cast<int>(result.code), result.message);
    });

    router.registerHandler(QStringLiteral("admin.charger.list"),
                           [&](const JsonRequest &request) {
        qint64 adminId = 0;
        JsonResponse failure;
        if (!resolveAdmin(request, sessionManager, &adminId, &failure)) return failure;
        const QJsonValue statusValue = request.data.value(QStringLiteral("status"));
        int status = -1;
        if (!statusValue.isUndefined()) {
            if (!statusValue.isDouble() || statusValue.toDouble() != statusValue.toInteger()) {
                return invalidParameter(adminService, adminId, QStringLiteral("admin.charger.list"), QStringLiteral("status"), QStringLiteral("expected integer enum"), request.requestId);
            }
            status = statusValue.toInt(-2);
            if (status != -1 && (status < 0 || status > 2)) return invalidParameter(adminService, adminId, QStringLiteral("admin.charger.list"), QStringLiteral("status"), QStringLiteral("enum out of range"), request.requestId);
        }
        qint64 stationId = -1;
        if (!request.data.value(QStringLiteral("station_id")).isUndefined()
            && !positiveId(request.data.value(QStringLiteral("station_id")), &stationId)) return invalidParameter(adminService, adminId, QStringLiteral("admin.charger.list"), QStringLiteral("station_id"), QStringLiteral("expected positive integer"), request.requestId);
        const auto result = adminService.chargerList(
            adminId, request.data.value(QStringLiteral("keyword")).toString(), status, stationId);
        if (!result.success) {
            return JsonProtocol::failure(request.requestId,
                static_cast<int>(result.code), result.message);
        }
        QJsonArray items;
        for (const Charger &charger : result.value) items.append(chargerJson(charger));
        return JsonProtocol::success(request.requestId,
                                     {{QStringLiteral("items"), items}});
    });

    const auto chargerAction = [&](const QString &route,
                                   const std::function<ServiceResult<Charger>(qint64, qint64)> &action) {
        router.registerHandler(route, [&, route, action](const JsonRequest &request) {
            qint64 adminId = 0;
            JsonResponse failure;
            if (!resolveAdmin(request, sessionManager, &adminId, &failure)) return failure;
            qint64 chargerId = 0;
            if (!positiveId(request.data.value(QStringLiteral("charger_id")), &chargerId)) return invalidParameter(adminService, adminId, route, QStringLiteral("charger_id"), QStringLiteral("expected positive integer"), request.requestId);
            const auto result = action(adminId, chargerId);
            return result.success
                ? JsonProtocol::success(request.requestId,
                    {{QStringLiteral("charger"), chargerJson(result.value)}})
                : JsonProtocol::failure(request.requestId,
                    static_cast<int>(result.code), result.message);
        });
    };

    router.registerHandler(QStringLiteral("admin.charger.create"),
                           [&](const JsonRequest &request) {
        qint64 adminId = 0;
        JsonResponse failure;
        if (!resolveAdmin(request, sessionManager, &adminId, &failure)) return failure;
        qint64 stationId = 0;
        QString parameter, reason;
        if (!validAdminChargerInput(request.data, &parameter, &reason))
            return invalidParameter(adminService, adminId, QStringLiteral("admin.charger.create"), parameter, reason, request.requestId);
        stationId = request.data.value(QStringLiteral("station_id")).toInteger();
        const QJsonValue typeValue = request.data.value(QStringLiteral("type"));
        const QJsonValue powerValue = request.data.value(QStringLiteral("power_kw"));
        const auto result = adminService.createCharger(
            adminId, stationId, request.data.value(QStringLiteral("code")).toString(),
            typeValue.toInt(), powerValue.toDouble());
        return result.success
            ? JsonProtocol::success(request.requestId,
                {{QStringLiteral("charger"), chargerJson(result.value)}})
            : JsonProtocol::failure(request.requestId,
                static_cast<int>(result.code), result.message);
    });

    chargerAction(QStringLiteral("admin.charger.delete"),
                  [&](qint64 admin, qint64 id) { return adminService.deleteCharger(admin, id); });
    chargerAction(QStringLiteral("admin.charger.markFault"),
                  [&](qint64 admin, qint64 id) { return adminService.markChargerFault(admin, id); });
    chargerAction(QStringLiteral("admin.charger.recover"),
                  [&](qint64 admin, qint64 id) { return adminService.recoverCharger(admin, id); });
    chargerAction(QStringLiteral("admin.charger.restart"),
                  [&](qint64 admin, qint64 id) { return adminService.restartCharger(admin, id); });

    router.registerHandler(QStringLiteral("admin.station.list"), [&](const JsonRequest &request) {
        qint64 adminId = 0; JsonResponse failure;
        if (!resolveAdmin(request, sessionManager, &adminId, &failure)) return failure;
        const auto result = adminService.stationList(adminId, request.data.value(QStringLiteral("keyword")).toString());
        if (!result.success) return JsonProtocol::failure(request.requestId, static_cast<int>(result.code), result.message);
        QJsonArray items; for (const Station &station : result.value) items.append(stationJson(station));
        return JsonProtocol::success(request.requestId, {{QStringLiteral("stations"), items}});
    });

    const auto stationPayload = [](const QJsonObject &data, qint64 id) {
        Station station; station.id = id; station.name = data.value(QStringLiteral("name")).toString();
        station.address = data.value(QStringLiteral("address")).toString();
        station.longitude = data.value(QStringLiteral("longitude")).toDouble();
        station.latitude = data.value(QStringLiteral("latitude")).toDouble();
        station.price = data.value(QStringLiteral("price")).toDouble();
        station.totalSlots = data.value(QStringLiteral("total_slots")).toInt(-1);
        return station;
    };
    const auto stationWrite = [&](const QString &route, bool update) {
        router.registerHandler(route, [&, route, update, stationPayload](const JsonRequest &request) {
            qint64 adminId = 0; JsonResponse failure;
            if (!resolveAdmin(request, sessionManager, &adminId, &failure)) return failure;
            qint64 id = 0;
            if (update && !positiveId(request.data.value(QStringLiteral("station_id")), &id)) return invalidParameter(adminService, adminId, route, QStringLiteral("station_id"), QStringLiteral("expected positive integer"), request.requestId);
            QString parameter, reason;
            if (!validAdminStationInput(request.data, &parameter, &reason))
                return invalidParameter(adminService, adminId, route, parameter, reason, request.requestId);
            const auto result = update ? adminService.updateStation(adminId, stationPayload(request.data, id))
                                       : adminService.createStation(adminId, stationPayload(request.data, 0));
            return result.success ? JsonProtocol::success(request.requestId, {{QStringLiteral("station"), stationJson(result.value)}})
                                  : JsonProtocol::failure(request.requestId, static_cast<int>(result.code), result.message);
        });
    };
    stationWrite(QStringLiteral("admin.station.create"), false);
    stationWrite(QStringLiteral("admin.station.update"), true);
    router.registerHandler(QStringLiteral("admin.station.delete"), [&](const JsonRequest &request) {
        qint64 adminId = 0; JsonResponse failure;
        if (!resolveAdmin(request, sessionManager, &adminId, &failure)) return failure;
        qint64 id = 0;
        if (!positiveId(request.data.value(QStringLiteral("station_id")), &id)) return invalidParameter(adminService, adminId, QStringLiteral("admin.station.delete"), QStringLiteral("station_id"), QStringLiteral("expected positive integer"), request.requestId);
        const auto result = adminService.deleteStation(adminId, id);
        return result.success ? JsonProtocol::success(request.requestId, {{QStringLiteral("station"), stationJson(result.value)}})
                              : JsonProtocol::failure(request.requestId, static_cast<int>(result.code), result.message);
    });
    router.registerHandler(QStringLiteral("admin.station.batchCreateChargers"), [&](const JsonRequest &request) {
        qint64 adminId = 0; JsonResponse failure;
        if (!resolveAdmin(request, sessionManager, &adminId, &failure)) return failure;
        QString parameter, reason;
        if (!validAdminBatchChargerInput(request.data, &parameter, &reason))
            return invalidParameter(adminService, adminId, QStringLiteral("admin.station.batchCreateChargers"), parameter, reason, request.requestId);
        const qint64 stationId = request.data.value(QStringLiteral("station_id")).toInteger();
        const int count = request.data.value(QStringLiteral("count")).toInt();
        const auto result = adminService.batchCreateChargers(adminId, stationId,
            request.data.value(QStringLiteral("prefix")).toString(), count,
            request.data.value(QStringLiteral("type")).toInt(0), request.data.value(QStringLiteral("power_kw")).toDouble(7));
        if (!result.success) return JsonProtocol::failure(request.requestId, static_cast<int>(result.code), result.message);
        QJsonArray items; for (const Charger &charger : result.value) items.append(chargerJson(charger));
        return JsonProtocol::success(request.requestId, {{QStringLiteral("items"), items}});
    });

    router.registerHandler(QStringLiteral("admin.user.list"), [&](const JsonRequest &request) {
        qint64 adminId = 0; JsonResponse failure;
        if (!resolveAdmin(request, sessionManager, &adminId, &failure)) return failure;
        const auto result = adminService.userList(adminId, request.data.value(QStringLiteral("keyword")).toString());
        if (!result.success) return JsonProtocol::failure(request.requestId, static_cast<int>(result.code), result.message);
        QJsonArray items; for (const User &user : result.value) items.append(userJson(user));
        return JsonProtocol::success(request.requestId, {{QStringLiteral("users"), items}});
    });

    const auto userStatusAction = [&](const QString &route, bool freeze) {
        router.registerHandler(route, [&, route, freeze](const JsonRequest &request) {
            qint64 adminId = 0; JsonResponse failure;
            if (!resolveAdmin(request, sessionManager, &adminId, &failure)) return failure;
            qint64 userId = 0;
            if (!positiveId(request.data.value(QStringLiteral("user_id")), &userId)) return invalidParameter(adminService, adminId, route, QStringLiteral("user_id"), QStringLiteral("expected positive integer"), request.requestId);
            const auto result = freeze ? adminService.freezeUser(adminId, userId)
                                       : adminService.unfreezeUser(adminId, userId);
            return result.success ? JsonProtocol::success(request.requestId, {{QStringLiteral("user"), userJson(result.value)}})
                                  : JsonProtocol::failure(request.requestId, static_cast<int>(result.code), result.message);
        });
    };
    userStatusAction(QStringLiteral("admin.user.freeze"), true);
    userStatusAction(QStringLiteral("admin.user.unfreeze"), false);
    router.registerHandler(QStringLiteral("admin.user.orders"), [&](const JsonRequest &request) {
        qint64 adminId = 0; JsonResponse failure;
        if (!resolveAdmin(request, sessionManager, &adminId, &failure)) return failure;
        qint64 userId = 0;
        if (!positiveId(request.data.value(QStringLiteral("user_id")), &userId)) return invalidParameter(adminService, adminId, QStringLiteral("admin.user.orders"), QStringLiteral("user_id"), QStringLiteral("expected positive integer"), request.requestId);
        const auto result = adminService.userOrders(adminId, userId);
        if (!result.success) return JsonProtocol::failure(request.requestId, static_cast<int>(result.code), result.message);
        QJsonArray items; for (const ChargingRecord &record : result.value) items.append(orderJson(record));
        return JsonProtocol::success(request.requestId, {{QStringLiteral("user_id"), userId},
                                                         {QStringLiteral("orders"), items}});
    });
    registerPredictionRoutes(router, adminService, sessionManager);
}}
