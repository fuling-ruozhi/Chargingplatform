#include "admin_client_facade.h"
#include "model/business_error.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace ncs {

void AdminClientFacade::requestUsers(const QString &keyword)
{
    send(QStringLiteral("admin.user.list"), {{QStringLiteral("admin_session_token"), sessionToken_},
        {QStringLiteral("keyword"), keyword}});
}

void AdminClientFacade::freezeUser(qint64 userId)
{
    send(QStringLiteral("admin.user.freeze"), {{QStringLiteral("admin_session_token"), sessionToken_},
        {QStringLiteral("user_id"), userId}});
}

void AdminClientFacade::unfreezeUser(qint64 userId)
{
    send(QStringLiteral("admin.user.unfreeze"), {{QStringLiteral("admin_session_token"), sessionToken_},
        {QStringLiteral("user_id"), userId}});
}

void AdminClientFacade::requestUserOrders(qint64 userId)
{
    send(QStringLiteral("admin.user.orders"), {{QStringLiteral("admin_session_token"), sessionToken_},
        {QStringLiteral("user_id"), userId}});
}

void AdminClientFacade::handleUserResponse(const QString &route, const JsonResponse &response)
{
    if (route == QStringLiteral("admin.user.list")) {
        const QJsonValue value = response.data.value(QStringLiteral("users"));
        if (!value.isArray()) { emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument), QStringLiteral("服务器响应格式错误"), 0); return; }
        QVector<User> users;
        for (const QJsonValue &item : value.toArray()) {
            const QJsonObject object = item.toObject();
            if (!object.value(QStringLiteral("id")).isDouble()) { emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument), QStringLiteral("服务器响应格式错误"), 0); return; }
            users.append({object.value(QStringLiteral("id")).toInteger(), object.value(QStringLiteral("username")).toString(), object.value(QStringLiteral("phone")).toString(), object.value(QStringLiteral("nickname")).toString(), QString(), 0.0, object.value(QStringLiteral("status")).toInt(-1), object.value(QStringLiteral("created_at")).toString()});
        }
        emit usersReceived(users);
    } else if (route == QStringLiteral("admin.user.freeze") || route == QStringLiteral("admin.user.unfreeze")) {
        emit userActionSucceeded(route, response.data.value(QStringLiteral("user")).toObject().value(QStringLiteral("id")).toInteger());
    } else {
        const qint64 userId = response.data.value(QStringLiteral("user_id")).toInteger();
        const QJsonValue value = response.data.value(QStringLiteral("orders"));
        if (!value.isArray()) { emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument), QStringLiteral("服务器响应格式错误"), 0); return; }
        QVector<ChargingRecord> orders;
        for (const QJsonValue &item : value.toArray()) {
            const QJsonObject object = item.toObject();
            ChargingRecord record; record.id = object.value(QStringLiteral("id")).toInteger(); record.orderNo = object.value(QStringLiteral("order_no")).toString(); record.userId = object.value(QStringLiteral("user_id")).toInteger(); record.chargerId = object.value(QStringLiteral("charger_id")).toInteger(); record.chargerCode = object.value(QStringLiteral("charger_code")).toString(); record.stationId = object.value(QStringLiteral("station_id")).toInteger(); record.stationName = object.value(QStringLiteral("station_name")).toString(); record.startTime = object.value(QStringLiteral("start_time")).toString(); record.endTime = object.value(QStringLiteral("end_time")).toString(); record.cost = object.value(QStringLiteral("amount")).toDouble(); record.status = static_cast<ChargingOrderStatus>(object.value(QStringLiteral("status")).toInt()); orders.append(record);
        }
        emit userOrdersReceived(userId, orders);
    }
}

}
