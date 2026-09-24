#include "admin_client_facade.h"

#include "model/business_error.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonValue>
#include <cmath>

namespace ncs {
namespace {

bool parseCharger(const QJsonObject &object, Charger *charger)
{
    const QJsonValue id = object.value(QStringLiteral("id"));
    const QJsonValue stationId = object.value(QStringLiteral("station_id"));
    const QJsonValue status = object.value(QStringLiteral("status"));
    const QJsonValue type = object.value(QStringLiteral("type"));
    const QJsonValue power = object.value(QStringLiteral("power_kw"));
    if (!id.isDouble() || id.toDouble() <= 0.0 || !stationId.isDouble()
        || stationId.toDouble() <= 0.0 || !status.isDouble()
        || status.toInt(-1) < 0 || status.toInt(-1) > 2 || !type.isDouble()
        || !power.isDouble() || power.toDouble() <= 0.0
        || !object.value(QStringLiteral("code")).isString()
        || !object.value(QStringLiteral("station_name")).isString()) return false;
    charger->id = id.toInteger();
    charger->stationId = stationId.toInteger();
    charger->stationName = object.value(QStringLiteral("station_name")).toString();
    charger->code = object.value(QStringLiteral("code")).toString();
    charger->status = static_cast<ChargerStatus>(status.toInt());
    charger->activeOrderStatus = object.value(QStringLiteral("active_order_status")).toInt(-1);
    charger->type = type.toInt();
    charger->powerKw = power.toDouble();
    charger->totalCount = object.value(QStringLiteral("total_count")).toInt();
    charger->totalMinutes = object.value(QStringLiteral("total_minutes")).toInteger();
    return true;
}

}

void AdminClientFacade::handle(const JsonResponse &response)
{
    const auto pending = requests_.take(response.requestId);
    if (pending.route.isEmpty()) return;
    const QString route = pending.route;
    if (!response.success) {
        emit requestFailed(route, response.code, response.message,
            response.data.value(QStringLiteral("retry_after_seconds")).toInt());
        return;
    }
    if (route == QStringLiteral("admin.login")) {
        sessionToken_ = response.data.value(
            QStringLiteral("admin_session_token")).toString();
        const QJsonObject object = response.data.value(
            QStringLiteral("admin")).toObject();
        emit loginSucceeded({object.value(QStringLiteral("id")).toInteger(),
                             object.value(QStringLiteral("username")).toString(),
                             object.value(QStringLiteral("created_at")).toString()});
    } else if (route == QStringLiteral("admin.logout")) {
        sessionToken_.clear();
        emit logoutSucceeded();
    } else if (route == QStringLiteral("admin.summary")) {
        emit summaryReceived({
            response.data.value(QStringLiteral("database_path")).toString(),
            response.data.value(QStringLiteral("online_chargers")).toInt(),
            response.data.value(QStringLiteral("total_chargers")).toInt()});
    } else if (route == QStringLiteral("admin.revenue.summary")) {
        const auto valid = [](const QJsonValue &value) {
            return value.isDouble() && std::isfinite(value.toDouble())
                && value.toDouble() >= 0.0;
        };
        if (!valid(response.data.value(QStringLiteral("todayRevenue")))
            || !valid(response.data.value(QStringLiteral("monthRevenue")))
            || !valid(response.data.value(QStringLiteral("totalRevenue")))) {
            emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument),
                               QStringLiteral("服务器响应格式错误"), 0);
            return;
        }
        emit revenueSummaryReceived({
            response.data.value(QStringLiteral("todayRevenue")).toDouble(),
            response.data.value(QStringLiteral("monthRevenue")).toDouble(),
            response.data.value(QStringLiteral("totalRevenue")).toDouble()});
    } else if (route == QStringLiteral("admin.revenue.trend")) {
        const QJsonValue daysValue = response.data.value(QStringLiteral("days"));
        const QJsonValue itemsValue = response.data.value(QStringLiteral("items"));
        if (!daysValue.isDouble() || daysValue.toInt(-1) != pending.days
            || daysValue.toDouble() != pending.days || !itemsValue.isArray()) {
            emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument),
                               QStringLiteral("服务器响应格式错误"), 0);
            return;
        }
        const QJsonArray items = itemsValue.toArray();
        if (items.size() != pending.days) {
            emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument),
                               QStringLiteral("服务器响应格式错误"), 0);
            return;
        }
        const auto validNumber = [](const QJsonValue &value, bool integer) {
            if (!value.isDouble() || !std::isfinite(value.toDouble())
                || value.toDouble() < 0.0) return false;
            return !integer || std::floor(value.toDouble()) == value.toDouble();
        };
        RevenueTrend trend;
        trend.days = pending.days;
        for (const auto &item : items) {
            if (!item.isObject()) {
                emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument),
                                   QStringLiteral("服务器响应格式错误"), 0);
                return;
            }
            const QJsonObject object = item.toObject();
            const QString dateText = object.value(QStringLiteral("date")).toString();
            const QDate date = QDate::fromString(dateText, Qt::ISODate);
            if (!date.isValid() || dateText != date.toString(Qt::ISODate)
                || !validNumber(object.value(QStringLiteral("revenue")), false)
                || !validNumber(object.value(QStringLiteral("order_count")), true)
                || (!trend.items.isEmpty() && trend.items.last().date.addDays(1) != date)) {
                emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument),
                                   QStringLiteral("服务器响应格式错误"), 0);
                return;
            }
            trend.items.append({date, object.value(QStringLiteral("revenue")).toDouble(),
                                object.value(QStringLiteral("order_count")).toInteger()});
        }
        emit revenueTrendReceived(trend);
    } else if (route == QStringLiteral("admin.revenue.recentOrders")) {
        const QJsonValue itemsValue = response.data.value(QStringLiteral("items"));
        if (!itemsValue.isArray() || itemsValue.toArray().size() > 10) {
            emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument),
                               QStringLiteral("服务器响应格式错误"), 0);
            return;
        }
        RecentOrders orders;
        for (const auto &item : itemsValue.toArray()) {
            if (!item.isObject()) {
                emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument),
                                   QStringLiteral("服务器响应格式错误"), 0);
                return;
            }
            const QJsonObject object = item.toObject();
            const auto validTime = [&](const QString &key) {
                const QJsonValue value = object.value(key);
                return value.isString()
                    && QDateTime::fromString(value.toString(), Qt::ISODateWithMs).isValid();
            };
            const QJsonValue id = object.value(QStringLiteral("id"));
            const QJsonValue cost = object.value(QStringLiteral("cost"));
            if (!id.isDouble() || id.toDouble() <= 0.0
                || std::floor(id.toDouble()) != id.toDouble()
                || !cost.isDouble() || !std::isfinite(cost.toDouble()) || cost.toDouble() < 0.0
                || !object.value(QStringLiteral("charger_code")).isString()
                || !object.value(QStringLiteral("station_name")).isString()
                || !validTime(QStringLiteral("start_time"))
                || !validTime(QStringLiteral("end_time"))) {
                emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument),
                                   QStringLiteral("服务器响应格式错误"), 0);
                return;
            }
            orders.append({id.toInteger(), object.value(QStringLiteral("charger_code")).toString(),
                           object.value(QStringLiteral("station_name")).toString(),
                           object.value(QStringLiteral("start_time")).toString(),
                           object.value(QStringLiteral("end_time")).toString(), cost.toDouble()});
        }
        emit recentOrdersReceived(orders);
    } else if (route == QStringLiteral("admin.charger.statusSummary")) {
        const auto validInteger = [&](const QString &key) {
            const QJsonValue value = response.data.value(key);
            return value.isDouble() && std::isfinite(value.toDouble())
                && value.toDouble() >= 0.0 && std::floor(value.toDouble()) == value.toDouble();
        };
        const auto validPercent = [&](const QString &key) {
            const QJsonValue value = response.data.value(key);
            return value.isDouble() && std::isfinite(value.toDouble())
                && value.toDouble() >= 0.0 && value.toDouble() <= 100.0;
        };
        if (!validInteger(QStringLiteral("total")) || !validInteger(QStringLiteral("idle"))
            || !validInteger(QStringLiteral("in_use")) || !validInteger(QStringLiteral("fault"))
            || !validPercent(QStringLiteral("health"))
            || !validPercent(QStringLiteral("idle_percent"))
            || !validPercent(QStringLiteral("in_use_percent"))
            || !validPercent(QStringLiteral("fault_percent"))) {
            emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument),
                               QStringLiteral("服务器响应格式错误"), 0);
            return;
        }
        ChargerStatusSummary summary{
            response.data.value(QStringLiteral("total")).toInteger(),
            response.data.value(QStringLiteral("idle")).toInteger(),
            response.data.value(QStringLiteral("in_use")).toInteger(),
            response.data.value(QStringLiteral("fault")).toInteger(),
            response.data.value(QStringLiteral("idle_percent")).toDouble(),
            response.data.value(QStringLiteral("in_use_percent")).toDouble(),
            response.data.value(QStringLiteral("fault_percent")).toDouble(),
            response.data.value(QStringLiteral("health")).toDouble()};
        const auto matches = [](double actual, qint64 count, qint64 total) {
            return std::abs(actual - (total ? 100.0 * count / total : 0.0)) <= 0.011;
        };
        if (summary.total != summary.idle + summary.inUse + summary.fault
            || !matches(summary.health, summary.idle + summary.inUse, summary.total)
            || !matches(summary.idlePercent, summary.idle, summary.total)
            || !matches(summary.inUsePercent, summary.inUse, summary.total)
            || !matches(summary.faultPercent, summary.fault, summary.total)) {
            emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument),
                               QStringLiteral("服务器响应格式错误"), 0);
            return;
        }
        emit chargerStatusReceived(summary);
    } else if (route == QStringLiteral("admin.charger.list")) {
        const QJsonValue items = response.data.value(QStringLiteral("items"));
        if (!items.isArray()) {
            emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument),
                               QStringLiteral("服务器响应格式错误"), 0);
            return;
        }
        QVector<Charger> chargers;
        for (const QJsonValue &item : items.toArray()) {
            Charger charger;
            if (!item.isObject() || !parseCharger(item.toObject(), &charger)) {
                emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument),
                                   QStringLiteral("服务器响应格式错误"), 0);
                return;
            }
            chargers.append(charger);
        }
        emit chargersReceived(chargers);
    } else if (route == QStringLiteral("admin.station.list")) {
        const QJsonValue items = response.data.value(QStringLiteral("stations"));
        if (!items.isArray()) {
            emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument),
                               QStringLiteral("服务器响应格式错误"), 0);
            return;
        }
        QVector<Station> stations;
        for (const auto &item : items.toArray()) {
            const auto object = item.toObject();
            Station station{object.value(QStringLiteral("id")).toInteger(),
                            object.value(QStringLiteral("name")).toString(),
                            object.value(QStringLiteral("address")).toString(),
                            object.value(QStringLiteral("price")).toDouble(),
                            object.value(QStringLiteral("total_slots")).toInt()};
            station.longitude = object.value(QStringLiteral("longitude")).toDouble();
            station.latitude = object.value(QStringLiteral("latitude")).toDouble();
            station.chargerCount = object.value(QStringLiteral("charger_count")).toInt();
            station.idleSlots = object.value(QStringLiteral("idle_slots")).toInt();
            const auto rating = object.value(QStringLiteral("rating_summary")).toObject();
            station.reviewCount = rating.value(QStringLiteral("review_count")).toInteger();
            station.averageScore = rating.value(QStringLiteral("average_score")).toDouble();
            station.environmentScore = rating.value(QStringLiteral("environment_score")).toDouble();
            station.queueScore = rating.value(QStringLiteral("queue_score")).toDouble();
            station.equipmentScore = rating.value(QStringLiteral("equipment_score")).toDouble();
            station.parkingScore = rating.value(QStringLiteral("parking_score")).toDouble();
            stations.append(station);
        }
        emit stationsReceived(stations);
    } else if (route == QStringLiteral("admin.station.batchCreateChargers")) {
        emit batchChargersSucceeded(response.data.value(QStringLiteral("items")).toArray().size());
    } else if (route.startsWith(QStringLiteral("admin.station."))) {
        const auto station = response.data.value(QStringLiteral("station")).toObject();
        emit stationActionSucceeded(route, station.value(QStringLiteral("id")).toInteger());
    } else if (route.startsWith(QStringLiteral("admin.user."))) {
        handleUserResponse(route, response);
    } else if (route.startsWith(QStringLiteral("admin.charger."))
               && route != QStringLiteral("admin.charger.statusSummary")) {
        const QJsonValue charger = response.data.value(QStringLiteral("charger"));
        const QJsonValue id = charger.isObject()
            ? charger.toObject().value(QStringLiteral("id")) : QJsonValue();
        if (!charger.isObject() || !id.isDouble() || id.toDouble() <= 0.0) {
            emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument),
                               QStringLiteral("服务器响应格式错误"), 0);
            return;
        }
        emit chargerActionSucceeded(route, id.toInteger());
    }
}

}
