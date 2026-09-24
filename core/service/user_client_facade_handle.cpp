#include "user_client_facade.h"

#include "model/business_error.h"

#include <QJsonArray>

namespace ncs {
namespace {

ChargingRecord recordFromJson(const QJsonObject &data)
{
    ChargingRecord record;
    record.id = data.value(QStringLiteral("order_id")).toInteger(
        data.value(QStringLiteral("id")).toInteger());
    record.orderNo = data.value(QStringLiteral("order_no")).toString();
    record.userId = data.value(QStringLiteral("user_id")).toInteger();
    record.chargerId = data.value(QStringLiteral("charger_id")).toInteger();
    record.startTime = data.value(QStringLiteral("start_time")).toString();
    record.endTime = data.value(QStringLiteral("end_time")).toString();
    record.energy = data.value(QStringLiteral("energy")).toDouble();
    record.cost = data.value(QStringLiteral("amount")).toDouble();
    record.durationSeconds = data.value(QStringLiteral("duration_seconds")).toInteger();
    record.status = static_cast<ChargingOrderStatus>(
        data.value(QStringLiteral("status")).toInt(
            static_cast<int>(ChargingOrderStatus::Charging)));
    record.reservedAt = data.value(QStringLiteral("reserved_at")).toString();
    record.expireAt = data.value(QStringLiteral("expire_at")).toString();
    record.stationId = data.value(QStringLiteral("station_id")).toInteger();
    record.stationName = data.value(QStringLiteral("station_name")).toString();
    record.chargerCode = data.value(QStringLiteral("charger_code")).toString();
    record.price = data.value(QStringLiteral("price_per_kwh")).toDouble();
    record.powerKw = data.value(QStringLiteral("power_kw")).toDouble();
    record.timeScale = data.value(QStringLiteral("time_scale")).toInt(60);
    record.initialSoc = data.value(QStringLiteral("initial_soc")).toDouble(20.0);
    record.finalSoc = data.value(QStringLiteral("final_soc")).toDouble(record.initialSoc);
    record.debtAmount = data.value(QStringLiteral("debt_amount")).toDouble();
    record.balanceAfter = data.value(QStringLiteral("balance_after")).toDouble();
    return record;
}

User userFromJson(const QJsonObject &data)
{
    User user;
    user.id = data.value(QStringLiteral("id")).toInteger();
    user.username = data.value(QStringLiteral("username")).toString();
    user.phone = data.value(QStringLiteral("phone_masked")).toString();
    user.nickname = data.value(QStringLiteral("nickname")).toString();
    user.avatarPath = data.value(QStringLiteral("avatar_path")).toString();
    user.balance = data.value(QStringLiteral("balance")).toDouble();
    user.status = data.value(QStringLiteral("status")).toInt(1);
    user.createdAt = data.value(QStringLiteral("created_at")).toString();
    return user;
}

}

void UserClientFacade::handle(const JsonResponse &response)
{
    const QString route = requests_.take(response.requestId);
    if (route.isEmpty()) return;
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
        emit loginSucceeded(userFromJson(response.data.value(QStringLiteral("user")).toObject()));
        return;
    }
    if (route == QStringLiteral("user.profile.get")) {
        emit profileReceived(userFromJson(response.data));
        return;
    }
    if (route == QStringLiteral("user.profile.nickname.update")
        || route == QStringLiteral("user.profile.avatar.update")) {
        emit profileUpdated(userFromJson(response.data));
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
        for (const QJsonValue &value : response.data.value(QStringLiteral("preferred_charger_types")).toArray()) {
            preference.preferredChargerTypes.append(value.toInt());
        }
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
        for (const QJsonValue &value : response.data.value(QStringLiteral("station_ids")).toArray()) {
            stationIds.append(value.toInteger());
        }
        emit favoriteStationsReceived(stationIds);
        return;
    }
    if (route == QStringLiteral("favorite_station.add")
        || route == QStringLiteral("favorite_station.remove")) {
        emit favoriteStationChanged(
            response.data.value(QStringLiteral("station_id")).toInteger(),
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
            for (const QJsonValue &type : object.value(QStringLiteral("matched_types")).toArray()) {
                match.matchedTypes.append(type.toInt());
            }
            matches.append(match);
        }
        emit remindersReceived(matches);
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
            station.distanceKm = object.value(QStringLiteral("distance_km")).toDouble();
            const QJsonObject rating = object.value(QStringLiteral("rating_summary")).toObject();
            station.reviewCount = rating.value(QStringLiteral("review_count")).toInteger();
            station.averageScore = rating.value(QStringLiteral("average_score")).toDouble();
            station.queueScore = rating.value(QStringLiteral("queue_score")).toDouble();
            station.recommendScore = object.value(QStringLiteral("recommend_score")).toDouble();
            stations.append(station);
        }
        emit stationsReceived(stations);
        return;
    }
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
        detail.station.distanceKm = response.data.value(QStringLiteral("distance_km")).toDouble();
        const QJsonObject rating = response.data.value(QStringLiteral("rating_summary")).toObject();
        detail.ratingSummary.reviewCount = rating.value(QStringLiteral("review_count")).toInteger();
        detail.ratingSummary.averageScore = rating.value(QStringLiteral("average_score")).toDouble();
        detail.ratingSummary.environmentAverage = rating.value(QStringLiteral("environment_score")).toDouble();
        detail.ratingSummary.queueAverage = rating.value(QStringLiteral("queue_score")).toDouble();
        detail.ratingSummary.equipmentAverage = rating.value(QStringLiteral("equipment_score")).toDouble();
        detail.ratingSummary.parkingAverage = rating.value(QStringLiteral("parking_score")).toDouble();
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
        const ChargingRecord record = recordFromJson(response.data);
        if (route == QStringLiteral("charge.reserve")) emit reservationCreated(record);
        if (route == QStringLiteral("charge.start")) emit chargeStarted(record);
        if (route == QStringLiteral("charge.settle")) emit chargeStopped(record);
        if (route == QStringLiteral("charge.cancel")) emit reservationCancelled(record);
        return;
    }
    if (route == QStringLiteral("charge.active")) {
        const bool hasActive = response.data.value(QStringLiteral("has_active")).toBool();
        emit activeChargeReceived(hasActive,
                                  hasActive ? recordFromJson(response.data) : ChargingRecord());
        return;
    }
    if (route == QStringLiteral("order.list")) {
        QVector<ChargingRecord> records;
        for (const QJsonValue value : response.data.value(QStringLiteral("orders")).toArray())
            records.append(recordFromJson(value.toObject()));
        emit ordersReceived(records);
        return;
    }
    if (route == QStringLiteral("order.detail")) {
        emit orderDetailReceived(recordFromJson(response.data));
        return;
    }
    if (route == QStringLiteral("review.submit")
        || route == QStringLiteral("review.get")) {
        Review review;
        review.id = response.data.value(QStringLiteral("review_id")).toInteger();
        review.orderId = response.data.value(QStringLiteral("order_id")).toInteger();
        review.userId = response.data.value(QStringLiteral("user_id")).toInteger();
        review.stationId = response.data.value(QStringLiteral("station_id")).toInteger();
        review.chargerId = response.data.value(QStringLiteral("charger_id")).toInteger();
        review.environmentScore = response.data.value(QStringLiteral("environment_score")).toInt();
        review.queueScore = response.data.value(QStringLiteral("queue_score")).toInt();
        review.equipmentScore = response.data.value(QStringLiteral("equipment_score")).toInt();
        review.parkingScore = response.data.value(QStringLiteral("parking_score")).toInt();
        review.overallScore = response.data.value(QStringLiteral("overall_score")).toDouble();
        review.createdAt = response.data.value(QStringLiteral("created_at")).toString();
        if (route == QStringLiteral("review.submit")) emit reviewSubmitted(review);
        else emit reviewReceived(response.data.value(QStringLiteral("has_review")).toBool(), review);
        return;
    }
    if (route == QStringLiteral("legacy.user.register")) {
        emit registerSucceeded(userFromJson(response.data));
    } else if (route == QStringLiteral("legacy.user.login")) {
        sessionToken_ = response.data.value(QStringLiteral("session_token")).toString();
        emit loginSucceeded(userFromJson(response.data.value(QStringLiteral("user")).toObject()));
    }
}

}
