#include "user_client_facade.h"

#include "model/business_error.h"
#include "user_client_facade_json.h"

#include <QJsonArray>
#include <QTimer>

namespace ncs {

UserClientFacade::UserClientFacade(QObject *parent) : QObject(parent), client_(this)
{
    requests_ = new RequestLifecycleManager(QStringLiteral("async-user"), this);
    qRegisterMetaType<ncs::User>();
    qRegisterMetaType<QVector<ncs::Station>>();
    qRegisterMetaType<QVector<ncs::StationRecommendation>>();
    qRegisterMetaType<ncs::StationDetail>();
    qRegisterMetaType<ncs::ChargingRecord>();
    qRegisterMetaType<ncs::ChargeForecast>();
    qRegisterMetaType<ncs::ChargingOrderStatus>();
    qRegisterMetaType<ncs::RechargeResult>();
    qRegisterMetaType<ncs::Review>();
    qRegisterMetaType<ncs::UserPreference>();
    qRegisterMetaType<QVector<qint64>>();
    qRegisterMetaType<QVector<ncs::ReminderMatch>>();
    connect(&client_, &NetworkClient::connected, this, &UserClientFacade::connected);
    connect(&client_, &NetworkClient::disconnected, this, [this] {
        sessionToken_.clear();
        failCancelled(requests_->advanceSession(QStringLiteral("disconnect")),
                      QStringLiteral("网络连接已断开"));
        emit disconnected();
    });
    connect(&client_, &NetworkClient::networkError, this, &UserClientFacade::networkError);
    connect(&client_, &NetworkClient::responseReceived,
            this, &UserClientFacade::handle);
    connect(requests_, &RequestLifecycleManager::requestTimedOut, this,
            [this](const AsyncRequestContext &context) {
                emit requestFailed(
                    context.route,
                    static_cast<int>(ProtocolErrorCode::NetworkError),
                    QStringLiteral("request timeout"));
            });
    connect(requests_, &RequestLifecycleManager::duplicateSuppressed,
            this, &UserClientFacade::requestSuppressed);
}

UserClientFacade::~UserClientFacade()
{
    // NetworkClient closes its socket from its destructor. Disconnect first so
    // that close-time signals cannot access facade members already being torn
    // down by C++ member destruction.
    disconnect(&client_, nullptr, this, nullptr);
}

void UserClientFacade::connectToServer(const QString &host, quint16 port)
{
    client_.connectToServer(host, port);
}
void UserClientFacade::disconnectFromServer() { client_.disconnectFromServer(); }
bool UserClientFacade::isConnected() const { return client_.isConnected(); }

void UserClientFacade::send(const QString &route, const QJsonObject &data)
{
    const bool transactional = route == QStringLiteral("user.otp.request")
        || route == QStringLiteral("user.login")
        || route == QStringLiteral("user.profile.nickname.update")
        || route == QStringLiteral("user.profile.avatar.update")
        || route == QStringLiteral("user.recharge")
        || route == QStringLiteral("user.logout")
        || route == QStringLiteral("charge.reserve")
        || route == QStringLiteral("charge.start")
        || route == QStringLiteral("charge.settle")
        || route == QStringLiteral("charge.cancel")
        || route.startsWith(QStringLiteral("legacy.user."));
    const auto policy = transactional
        ? RequestLifecycleManager::DuplicatePolicy::Reject
        : RequestLifecycleManager::DuplicatePolicy::Supersede;
    if (!requests_->prepare(route, policy)) return;
    const QString id = client_.sendRequest(route, data);
    if (id.isEmpty()) {
        emit requestFailed(route, 2001, QStringLiteral("服务器未连接，请先启动 ncs_admin"));
    } else {
        requests_->track(id, route, RequestLifecycleManager::DuplicatePolicy::Allow);
    }
}

void UserClientFacade::expireRequest(const QString &requestId)
{
    requests_->expireNow(requestId);
}

void UserClientFacade::sendAuthenticated(const QString &route, QJsonObject data)
{
    if (sessionToken_.isEmpty()) {
        emit requestFailed(route,
                           static_cast<int>(BusinessErrorCode::AuthRequired),
                           QStringLiteral("登录状态已失效，请重新登录"));
        return;
    }
    data.insert(QStringLiteral("session_token"), sessionToken_);
    send(route, data);
}

void UserClientFacade::send(const QString &route, const QString &username,
                            const QString &password)
{
    send(route, {{QStringLiteral("username"), username},
                 {QStringLiteral("password"), password}});
}

void UserClientFacade::registerUser(const QString &username, const QString &password)
{
    send(QStringLiteral("legacy.user.register"), username, password);
}

void UserClientFacade::legacyLogin(const QString &username, const QString &password)
{
    send(QStringLiteral("legacy.user.login"), username, password);
}

void UserClientFacade::requestOtp(const QString &phone)
{
    send(QStringLiteral("user.otp.request"),
         {{QStringLiteral("phone"), phone},
          {QStringLiteral("demo_otp_echo"), true}});
}

void UserClientFacade::login(const QString &phone, const QString &code)
{
    send(QStringLiteral("user.login"),
         {{QStringLiteral("phone"), phone}, {QStringLiteral("code"), code}});
}

void UserClientFacade::requestProfile()
{
    sendAuthenticated(QStringLiteral("user.profile.get"));
}

void UserClientFacade::requestPreference()
{
    sendAuthenticated(QStringLiteral("preference.get"));
}

void UserClientFacade::updatePreference(const UserPreference &preference)
{
    QJsonArray types;
    for (const int type : preference.preferredChargerTypes) types.append(type);
    QJsonObject data{{QStringLiteral("home_radius_km"), preference.homeRadiusKm},
                     {QStringLiteral("preferred_charger_types"), types},
                     {QStringLiteral("reminder_start_time"), preference.reminderStartTime},
                     {QStringLiteral("reminder_end_time"), preference.reminderEndTime},
                     {QStringLiteral("min_idle_chargers"), preference.minIdleChargers},
                     {QStringLiteral("dnd_start_time"), preference.dndStartTime},
                     {QStringLiteral("dnd_end_time"), preference.dndEndTime},
                     {QStringLiteral("enabled"), preference.enabled}};
    if (preference.hasHomeLocation) {
        data.insert(QStringLiteral("home_latitude"), preference.homeLatitude);
        data.insert(QStringLiteral("home_longitude"), preference.homeLongitude);
    } else {
        data.insert(QStringLiteral("home_latitude"), QJsonValue(QJsonValue::Null));
        data.insert(QStringLiteral("home_longitude"), QJsonValue(QJsonValue::Null));
    }
    sendAuthenticated(QStringLiteral("preference.update"), data);
}

void UserClientFacade::requestFavoriteStations()
{
    sendAuthenticated(QStringLiteral("favorite_station.list"));
}

void UserClientFacade::addFavoriteStation(qint64 stationId)
{
    sendAuthenticated(QStringLiteral("favorite_station.add"),
                      {{QStringLiteral("station_id"), stationId}});
}

void UserClientFacade::removeFavoriteStation(qint64 stationId)
{
    sendAuthenticated(QStringLiteral("favorite_station.remove"),
                      {{QStringLiteral("station_id"), stationId}});
}

void UserClientFacade::checkReminders()
{
    sendAuthenticated(QStringLiteral("reminder.check"));
}

void UserClientFacade::handle(const JsonResponse &response)
{
    const auto context = requests_->complete(response.requestId);
    if (!context.has_value()) {
        requests_->consumeRetired(response.requestId);
        return;
    }
    const QString route = context->route;
    if (!response.success) {
        if (response.code == static_cast<int>(BusinessErrorCode::AuthRequired)
            || response.code == static_cast<int>(BusinessErrorCode::SessionExpired)) {
            emit sessionInvalid();
        }
        emit requestFailed(route, response.code, response.message);
        return;
    }
    if (route == QStringLiteral("user.otp.request")) {
        emit otpReceived(response.data.value(QStringLiteral("display_code")).toString(),
                         response.data.value(QStringLiteral("cooldown_seconds")).toInt(),
                         response.data.value(QStringLiteral("expires_seconds")).toInt());
        return;
    }
    if (route == QStringLiteral("user.login")) {
        sessionToken_ = response.data.value(QStringLiteral("session_token")).toString();
        requests_->sessionStarted();
        emit loginSucceeded(clientUserFromJson(
            response.data.value(QStringLiteral("user")).toObject()));
        return;
    }
    if (route == QStringLiteral("user.profile.get")) {
        emit profileReceived(clientUserFromJson(response.data));
        return;
    }
    if (route == QStringLiteral("preference.get")
        || route == QStringLiteral("preference.update")) {
        UserPreference preference;
        const QJsonValue latitude = response.data.value(QStringLiteral("home_latitude"));
        const QJsonValue longitude = response.data.value(QStringLiteral("home_longitude"));
        preference.hasHomeLocation = !latitude.isNull() && !longitude.isNull();
        preference.homeLatitudeSet = !latitude.isNull();
        preference.homeLongitudeSet = !longitude.isNull();
        preference.homeLatitude = latitude.toDouble();
        preference.homeLongitude = longitude.toDouble();
        preference.homeRadiusKm = response.data.value(QStringLiteral("home_radius_km")).toDouble(3.0);
        for (const QJsonValue &value : response.data.value(QStringLiteral("preferred_charger_types")).toArray())
            preference.preferredChargerTypes.append(value.toInt());
        preference.reminderStartTime = response.data.value(QStringLiteral("reminder_start_time")).toString();
        preference.reminderEndTime = response.data.value(QStringLiteral("reminder_end_time")).toString();
        preference.minIdleChargers = response.data.value(QStringLiteral("min_idle_chargers")).toInt(1);
        preference.dndStartTime = response.data.value(QStringLiteral("dnd_start_time")).toString();
        preference.dndEndTime = response.data.value(QStringLiteral("dnd_end_time")).toString();
        preference.enabled = response.data.value(QStringLiteral("enabled")).toBool(true);
        if (route == QStringLiteral("preference.get")) emit preferenceReceived(preference);
        else emit preferenceUpdated(preference);
        return;
    }
    if (route == QStringLiteral("favorite_station.list")) {
        QVector<qint64> stationIds;
        for (const QJsonValue &value : response.data.value(QStringLiteral("station_ids")).toArray())
            stationIds.append(value.toInteger());
        emit favoriteStationsReceived(stationIds);
        return;
    }
    if (route == QStringLiteral("favorite_station.add")
        || route == QStringLiteral("favorite_station.remove")) {
        emit favoriteStationChanged(response.data.value(QStringLiteral("station_id")).toInteger(),
                                    route == QStringLiteral("favorite_station.add"));
        return;
    }
    if (route == QStringLiteral("reminder.check")) {
        QVector<ReminderMatch> matches;
        for (const QJsonValue &value : response.data.value(QStringLiteral("alerts")).toArray()) {
            const QJsonObject object = value.toObject();
            ReminderMatch match;
            match.stationId = object.value(QStringLiteral("station_id")).toInteger();
            match.stationName = object.value(QStringLiteral("station_name")).toString();
            match.hasDistance = !object.value(QStringLiteral("distance_km")).isNull();
            match.distanceKm = object.value(QStringLiteral("distance_km")).toDouble();
            match.idleMatchedChargers = object.value(QStringLiteral("idle_chargers")).toInt();
            match.favorite = object.value(QStringLiteral("is_favorite")).toBool();
            match.insideHomeRadius = object.value(QStringLiteral("inside_home_radius")).toBool();
            match.reason = object.value(QStringLiteral("reason")).toString();
            for (const QJsonValue &type : object.value(QStringLiteral("matched_types")).toArray())
                match.matchedTypes.append(type.toInt());
            matches.append(match);
        }
        emit remindersReceived(matches);
        return;
    }
    if (route == QStringLiteral("user.forecast")) handleForecastResponse(response);
    if (route == QStringLiteral("user.profile.nickname.update")
        || route == QStringLiteral("user.profile.avatar.update")) {
        emit profileUpdated(clientUserFromJson(response.data));
        return;
    }
    if (route == QStringLiteral("user.recharge")) {
        RechargeResult result;
        result.logId = response.data.value(QStringLiteral("log_id")).toInteger();
        result.amount = response.data.value(QStringLiteral("amount")).toDouble();
        result.balanceBefore = response.data.value(QStringLiteral("balance_before")).toDouble();
        result.balanceAfter = response.data.value(QStringLiteral("balance_after")).toDouble();
        emit rechargeSucceeded(result);
        return;
    }
    if (route == QStringLiteral("user.logout")) {
        sessionToken_.clear();
        failCancelled(requests_->advanceSession(QStringLiteral("logout")),
                      QStringLiteral("会话已结束"));
        emit logoutSucceeded();
        return;
    }
    if (route == QStringLiteral("station.list")) {
        QVector<Station> stations;
        for (const QJsonValue value : response.data.value(QStringLiteral("stations")).toArray()) {
            const QJsonObject object = value.toObject();
            Station station{object.value(QStringLiteral("id")).toInteger(),
                            object.value(QStringLiteral("name")).toString(),
                            object.value(QStringLiteral("address")).toString(),
                            object.value(QStringLiteral("price")).toDouble(),
                            object.value(QStringLiteral("total_slots")).toInt()};
            station.longitude = object.value(QStringLiteral("longitude")).toDouble();
            station.latitude = object.value(QStringLiteral("latitude")).toDouble();
            station.idleSlots = object.value(QStringLiteral("idle_slots")).toInt();
            station.chargerCount = object.value(QStringLiteral("charger_count")).toInt();
            station.distanceKm = object.value(QStringLiteral("distance_km")).toDouble();
            stations.append(station);
        }
        emit stationsReceived(stations);
        return;
    }
    if (handleStationRecommendationResponse(response, route)) return;
    if (route == QStringLiteral("station.detail")) {
        StationDetail detail;
        detail.station = {response.data.value(QStringLiteral("id")).toInteger(),
                          response.data.value(QStringLiteral("name")).toString(),
                          response.data.value(QStringLiteral("address")).toString(),
                          response.data.value(QStringLiteral("price")).toDouble(),
                          response.data.value(QStringLiteral("total_slots")).toInt()};
        detail.station.longitude = response.data.value(QStringLiteral("longitude")).toDouble();
        detail.station.latitude = response.data.value(QStringLiteral("latitude")).toDouble();
        detail.station.idleSlots = response.data.value(QStringLiteral("idle_slots")).toInt();
        detail.station.chargerCount = response.data.value(QStringLiteral("charger_count")).toInt();
        detail.station.distanceKm = response.data.value(QStringLiteral("distance_km")).toDouble();
        for (const QJsonValue value : response.data.value(QStringLiteral("chargers")).toArray()) {
            const QJsonObject object = value.toObject();
            Charger charger{object.value(QStringLiteral("id")).toInteger(),
                            object.value(QStringLiteral("station_id")).toInteger(),
                            object.value(QStringLiteral("code")).toString(),
                            static_cast<ChargerStatus>(
                                object.value(QStringLiteral("status")).toInt()),
                            object.value(QStringLiteral("active_order_status")).toInt(-1)};
            charger.type = object.value(QStringLiteral("type")).toInt();
            charger.powerKw = object.value(QStringLiteral("power_kw")).toDouble();
            charger.totalCount = object.value(QStringLiteral("total_count")).toInt();
            charger.totalMinutes = object.value(QStringLiteral("total_minutes")).toInteger();
            detail.chargers.append(charger);
        }
        emit stationDetailReceived(detail);
        return;
    }
    if (route == QStringLiteral("charge.reserve")
        || route == QStringLiteral("charge.start")
        || route == QStringLiteral("charge.settle")
        || route == QStringLiteral("charge.cancel")) {
        const ChargingRecord record = clientRecordFromJson(response.data);
        // Reservation/start success opens ChargingDialog in the current UI.
        // Defer only those two notifications so QTcpSocket::readyRead can
        // return before the dialog enters its nested event loop.
        if (route == QStringLiteral("charge.reserve")) {
            QTimer::singleShot(0, this, [this, record] { emit reservationCreated(record); });
        }
        if (route == QStringLiteral("charge.start")) {
            QTimer::singleShot(0, this, [this, record] { emit chargeStarted(record); });
        }
        if (route == QStringLiteral("charge.settle")) emit chargeStopped(record);
        if (route == QStringLiteral("charge.cancel")) emit reservationCancelled(record);
        return;
    }
    if (route == QStringLiteral("charge.active")) {
        const bool hasActive = response.data.value(QStringLiteral("has_active")).toBool();
        emit activeChargeReceived(hasActive,
                                  hasActive ? clientRecordFromJson(response.data)
                                            : ChargingRecord());
        return;
    }
    if (route == QStringLiteral("order.list")) {
        QVector<ChargingRecord> records;
        for (const QJsonValue value : response.data.value(QStringLiteral("orders")).toArray()) {
            records.append(clientRecordFromJson(value.toObject()));
        }
        emit ordersReceived(records);
        return;
    }
    if (route == QStringLiteral("order.detail")) {
        emit orderDetailReceived(clientRecordFromJson(response.data));
        return;
    }
    if (route == QStringLiteral("user.vehicle.profile.get")
        || route == QStringLiteral("user.vehicle.profile.update")) {
        handleVehicleProfile(route, response);
        return;
    }
    if (route == QStringLiteral("legacy.user.register")) {
        emit registerSucceeded(clientUserFromJson(response.data));
    } else if (route == QStringLiteral("legacy.user.login")) {
        sessionToken_ = response.data.value(QStringLiteral("session_token")).toString();
        requests_->sessionStarted();
        emit loginSucceeded(clientUserFromJson(
            response.data.value(QStringLiteral("user")).toObject()));
    }
}
}
