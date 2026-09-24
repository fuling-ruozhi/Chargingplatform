#include "admin_client_facade.h"

#include "model/business_error.h"
#include <QDate>
#include <QJsonArray>
#include <QJsonValue>
#include <QTimer>
#include <QUuid>
#include <cmath>
namespace ncs {
namespace {
constexpr int kAdminRequestTimeoutMs = 10'000;
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
AdminClientFacade::AdminClientFacade(QObject *parent) : QObject(parent), client_(this)
{
    requests_ = new RequestLifecycleManager(QStringLiteral("async-admin"), this);
    qRegisterMetaType<ncs::Admin>();
    qRegisterMetaType<ncs::AdminSummary>();
    qRegisterMetaType<QVector<ncs::Station>>();
    qRegisterMetaType<ncs::PredictionBundle>();
    qRegisterMetaType<ncs::PredictionList>();
    connect(&client_, &NetworkClient::connected,
            this, &AdminClientFacade::connected);
    connect(&client_, &NetworkClient::disconnected, this, [this] {
        sessionToken_.clear();
        failCancelled(requests_->advanceSession(QStringLiteral("disconnect")),
                      QStringLiteral("网络连接已断开"));
        emit disconnected();
    });
    connect(&client_, &NetworkClient::networkError,
            this, &AdminClientFacade::networkError);
    connect(&client_, &NetworkClient::responseReceived,
            this, &AdminClientFacade::handle);
    connect(requests_, &RequestLifecycleManager::requestTimedOut, this,
            [this](const AsyncRequestContext &context) {
                requestMetadata_.remove(context.requestId);
                emit requestFailed(
                    context.route,
                    static_cast<int>(ProtocolErrorCode::NetworkError),
                    QStringLiteral("request timeout"), 0);
            });
    connect(requests_, &RequestLifecycleManager::duplicateSuppressed,
            this, &AdminClientFacade::requestSuppressed);
}
void AdminClientFacade::connectToServer(const QString &host, quint16 port)
{
    client_.connectToServer(host, port);
}
void AdminClientFacade::disconnectFromServer()
{
    client_.disconnectFromServer();
}
bool AdminClientFacade::isConnected() const
{
    return client_.isConnected();
}
void AdminClientFacade::send(const QString &route, QJsonObject data)
{
    if (!isConnected()) {
        emit requestFailed(route, 2001, QStringLiteral("服务器未连接"), 0);
        return;
    }
    const auto policy = route == QStringLiteral("admin.summary")
        ? RequestLifecycleManager::DuplicatePolicy::Supersede
        : RequestLifecycleManager::DuplicatePolicy::Reject;
    if (policy == RequestLifecycleManager::DuplicatePolicy::Supersede) {
        const auto ids = requestMetadata_.keys();
        for (const QString &id : ids) {
            if (requestMetadata_.value(id).route == route) {
                requestMetadata_.remove(id);
            }
        }
    }
    if (!requests_->prepare(route, policy)) return;
    const QString requestId = client_.sendRequest(route, data);
    if (requestId.isEmpty()) {
        emit requestFailed(route, 2001, QStringLiteral("服务器尚未就绪"), 0);
        return;
    }
    requests_->track(requestId, route, RequestLifecycleManager::DuplicatePolicy::Allow);
    requestMetadata_.insert(requestId, {route, 0});
}
void AdminClientFacade::sendAuthenticated(const QString &route)
{
    if (sessionToken_.isEmpty()) {
        emit requestFailed(route,
                           static_cast<int>(BusinessErrorCode::AuthRequired),
                           QStringLiteral("管理员登录状态已失效"), 0);
        return;
    }
    send(route, {{QStringLiteral("admin_session_token"), sessionToken_}});
}
void AdminClientFacade::login(const QString &username, const QString &password)
{
    send(QStringLiteral("admin.login"),
         {{QStringLiteral("username"), username},
          {QStringLiteral("password"), password}});
}

void AdminClientFacade::logout()
{
    sendAuthenticated(QStringLiteral("admin.logout"));
}

void AdminClientFacade::requestLogs(const QString &kind, const QString &keyword,
                                    const QString &type, const QString &result)
{
    pendingLogKind_ = kind;
    if (!sessionToken_.isEmpty()) {
        send(QStringLiteral("admin.logs.query"), {{"admin_session_token", sessionToken_}, {"kind", kind}, {"keyword", keyword}, {"type", type}, {"result", result}});
    }
}

void AdminClientFacade::failCancelled(
    const QVector<AsyncRequestContext> &contexts, const QString &message)
{
    for (const AsyncRequestContext &context : contexts) {
        requestMetadata_.remove(context.requestId);
        emit requestFailed(context.route,
                           static_cast<int>(ProtocolErrorCode::NetworkError),
                           message, 0);
    }
}

void AdminClientFacade::handle(const JsonResponse &response)
{
    const auto context = requests_->complete(response.requestId);
    if (!context.has_value()) {
        requestMetadata_.remove(response.requestId);
        requests_->consumeRetired(response.requestId);
        return;
    }
    const PendingRequest pending = requestMetadata_.take(response.requestId);
    const QString route = context->route;
    if (!response.success) {
        emit requestFailed(route, response.code, response.message,
            response.data.value(QStringLiteral("retry_after_seconds")).toInt());
        return;
    }
    if (route == QStringLiteral("admin.login")) {
        sessionToken_ = response.data.value(
            QStringLiteral("admin_session_token")).toString();
        requests_->sessionStarted();
        const QJsonObject object = response.data.value(
            QStringLiteral("admin")).toObject();
        emit loginSucceeded({object.value(QStringLiteral("id")).toInteger(),
                             object.value(QStringLiteral("username")).toString(),
                             object.value(QStringLiteral("created_at")).toString()});
    } else if (route == QStringLiteral("admin.logout")) {
        sessionToken_.clear();
        failCancelled(requests_->advanceSession(QStringLiteral("logout")),
                      QStringLiteral("管理员会话已结束"));
        emit logoutSucceeded();
    } else if (route == QStringLiteral("admin.logs.query")) {
        emit logsReceived(pendingLogKind_, response.data.value(QStringLiteral("items")).toArray());
    } else if (route == QStringLiteral("admin.summary")) {
        emit summaryReceived({
            response.data.value(QStringLiteral("database_path")).toString(),
            response.data.value(QStringLiteral("online_chargers")).toInt(),
            response.data.value(QStringLiteral("total_chargers")).toInt()});
    } else if (route == QStringLiteral("admin.revenue.summary")
               || route == QStringLiteral("admin.revenue.trend")
               || route == QStringLiteral("admin.revenue.recentOrders")) {
        if (route == QStringLiteral("admin.revenue.summary")) {
            emit revenueSummaryReceived({
                response.data.value(QStringLiteral("today_revenue")).toDouble(),
                response.data.value(QStringLiteral("month_revenue")).toDouble(),
                response.data.value(QStringLiteral("total_revenue")).toDouble()});
        } else if (route == QStringLiteral("admin.revenue.trend")) {
            RevenueTrend trend;
            trend.days = pending.days;
            const QJsonValue items = response.data.value(QStringLiteral("items"));
            if (items.isArray()) {
                for (const QJsonValue &item : items.toArray()) {
                    const QJsonObject object = item.toObject();
                    RevenueDay day;
                    day.date = QDate::fromString(
                        object.value(QStringLiteral("date")).toString(),
                        QStringLiteral("yyyy-MM-dd"));
                    day.revenue = object.value(QStringLiteral("revenue")).toDouble();
                    day.orderCount =
                        object.value(QStringLiteral("order_count")).toInteger();
                    trend.items.append(day);
                }
            }
            emit revenueTrendReceived(trend);
        } else {
            RecentOrders orders;
            const QJsonValue items = response.data.value(QStringLiteral("items"));
            if (items.isArray()) {
                for (const QJsonValue &item : items.toArray()) {
                    const QJsonObject object = item.toObject();
                    RecentOrder order;
                    order.id = object.value(QStringLiteral("id")).toInteger();
                    order.chargerCode =
                        object.value(QStringLiteral("charger_code")).toString();
                    order.stationName =
                        object.value(QStringLiteral("station_name")).toString();
                    order.startTime =
                        object.value(QStringLiteral("start_time")).toString();
                    order.endTime =
                        object.value(QStringLiteral("end_time")).toString();
                    order.cost = object.value(QStringLiteral("cost")).toDouble();
                    orders.append(order);
                }
            }
            emit recentOrdersReceived(orders);
        }
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
        if (!validInteger(QStringLiteral("total"))
            || !validInteger(QStringLiteral("idle"))
            || !validInteger(QStringLiteral("in_use"))
            || !validInteger(QStringLiteral("fault"))
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
        if (!items.isArray()) { emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument), QStringLiteral("服务器响应格式错误"), 0); return; }
        QVector<Station> stations;
        for (const auto &item : items.toArray()) {
            const auto object = item.toObject();
            Station station{object.value(QStringLiteral("id")).toInteger(), object.value(QStringLiteral("name")).toString(), object.value(QStringLiteral("address")).toString(), object.value(QStringLiteral("price")).toDouble(), object.value(QStringLiteral("total_slots")).toInt()};
            station.longitude = object.value(QStringLiteral("longitude")).toDouble(); station.latitude = object.value(QStringLiteral("latitude")).toDouble();
            station.chargerCount = object.value(QStringLiteral("charger_count")).toInt(); station.idleSlots = object.value(QStringLiteral("idle_slots")).toInt(); stations.append(station);
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
    } else if (route.startsWith(QStringLiteral("admin.prediction."))) handlePredictionResponse(route, response);
}
}
