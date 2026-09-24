#pragma once

#include "model/charging_record.h"
#include "model/user.h"

#include <QJsonObject>

namespace ncs {

inline ChargingRecord clientRecordFromJson(const QJsonObject &data)
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

inline User clientUserFromJson(const QJsonObject &data)
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
