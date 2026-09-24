#include "user_client_facade.h"

namespace ncs {

void UserClientFacade::failCancelled(
    const QVector<AsyncRequestContext> &contexts, const QString &message)
{
    for (const AsyncRequestContext &context : contexts) {
        emit requestFailed(context.route,
                           static_cast<int>(ProtocolErrorCode::NetworkError), message);
    }
}

void UserClientFacade::updateNickname(const QString &nickname)
{
    sendAuthenticated(QStringLiteral("user.profile.nickname.update"),
                      {{QStringLiteral("nickname"), nickname}});
}

void UserClientFacade::updateAvatar(const QString &avatarPath)
{
    sendAuthenticated(QStringLiteral("user.profile.avatar.update"),
                      {{QStringLiteral("avatar_path"), avatarPath}});
}

void UserClientFacade::recharge(double amount)
{
    sendAuthenticated(QStringLiteral("user.recharge"),
                      {{QStringLiteral("amount"), amount}});
}

void UserClientFacade::logout()
{
    sendAuthenticated(QStringLiteral("user.logout"));
}

void UserClientFacade::requestStations()
{
    sendAuthenticated(QStringLiteral("station.list"));
}

void UserClientFacade::requestStations(double longitude, double latitude)
{
    sendAuthenticated(QStringLiteral("station.list"),
                      {{QStringLiteral("longitude"), longitude},
                       {QStringLiteral("latitude"), latitude}});
}

void UserClientFacade::requestStationDetail(qint64 stationId)
{
    sendAuthenticated(QStringLiteral("station.detail"),
                      {{QStringLiteral("station_id"), stationId}});
}

void UserClientFacade::startCharge(qint64 userId, qint64 chargerId, qint64 recordId)
{
    Q_UNUSED(userId)
    QJsonObject data;
    if (recordId > 0) data.insert(QStringLiteral("order_id"), recordId);
    else data.insert(QStringLiteral("charger_id"), chargerId);
    sendAuthenticated(QStringLiteral("charge.start"), data);
}

void UserClientFacade::requestStationDetail(qint64 stationId, double longitude,
                                            double latitude)
{
    sendAuthenticated(QStringLiteral("station.detail"),
                      {{QStringLiteral("station_id"), stationId},
                       {QStringLiteral("origin_longitude"), longitude},
                       {QStringLiteral("origin_latitude"), latitude}});
}

void UserClientFacade::reserveCharge(qint64 userId, qint64 chargerId)
{
    Q_UNUSED(userId)
    sendAuthenticated(QStringLiteral("charge.reserve"),
                      {{QStringLiteral("charger_id"), chargerId}});
}

void UserClientFacade::stopCharge(qint64 recordId)
{
    sendAuthenticated(QStringLiteral("charge.settle"),
                      {{QStringLiteral("order_id"), recordId}});
}

void UserClientFacade::cancelReservation(qint64 userId, qint64 recordId)
{
    Q_UNUSED(userId)
    sendAuthenticated(QStringLiteral("charge.cancel"),
                      {{QStringLiteral("order_id"), recordId}});
}

void UserClientFacade::requestActiveCharge(qint64 userId)
{
    Q_UNUSED(userId)
    sendAuthenticated(QStringLiteral("charge.active"), QJsonObject());
}

void UserClientFacade::requestOrders(int page, int pageSize)
{
    sendAuthenticated(QStringLiteral("order.list"),
                      {{QStringLiteral("page"), page},
                       {QStringLiteral("page_size"), pageSize}});
}

void UserClientFacade::requestOrderDetail(qint64 orderId)
{
    sendAuthenticated(QStringLiteral("order.detail"),
                      {{QStringLiteral("order_id"), orderId}});
}

void UserClientFacade::submitReview(qint64 orderId, int environmentScore, int queueScore,
                                    int equipmentScore, int parkingScore)
{
    sendAuthenticated(QStringLiteral("review.submit"),
                      {{QStringLiteral("order_id"), orderId},
                       {QStringLiteral("environment_score"), environmentScore},
                       {QStringLiteral("queue_score"), queueScore},
                       {QStringLiteral("equipment_score"), equipmentScore},
                       {QStringLiteral("parking_score"), parkingScore}});
}

void UserClientFacade::requestReview(qint64 orderId)
{
    sendAuthenticated(QStringLiteral("review.get"),
                      {{QStringLiteral("order_id"), orderId}});
}

}
