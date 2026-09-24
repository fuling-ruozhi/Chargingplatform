#include "admin_client_facade.h"

#include "model/business_error.h"

namespace ncs {

void AdminClientFacade::requestSummary()
{
    sendAuthenticated(QStringLiteral("admin.summary"));
}

void AdminClientFacade::sendAnalytics(const QString &route, QJsonObject data,
                                      int days)
{
    if (!isConnected()) {
        emit requestFailed(route, 2001, QStringLiteral("服务器未连接"), 0);
        return;
    }
    if (sessionToken_.isEmpty()) {
        emit requestFailed(route,
                           static_cast<int>(BusinessErrorCode::AuthRequired),
                           QStringLiteral("管理员登录状态已失效"), 0);
        return;
    }
    for (const PendingRequest &pending : requestMetadata_) {
        if (pending.route == route && pending.days == days) {
            emit requestSuppressed(route);
            return;
        }
    }
    data.insert(QStringLiteral("admin_session_token"), sessionToken_);
    const QString requestId = client_.sendRequest(route, data);
    if (requestId.isEmpty()) {
        emit requestFailed(route, 2001, QStringLiteral("请求发送失败"), 0);
        return;
    }
    requests_->track(requestId, route,
                     RequestLifecycleManager::DuplicatePolicy::Allow);
    requestMetadata_.insert(requestId, {route, days});
}

void AdminClientFacade::requestRevenueSummary()
{
    sendAnalytics(QStringLiteral("admin.revenue.summary"));
}

void AdminClientFacade::requestRevenueTrend(int days)
{
    sendAnalytics(QStringLiteral("admin.revenue.trend"),
                  {{QStringLiteral("days"), days}}, days);
}

void AdminClientFacade::requestRecentOrders()
{
    sendAnalytics(QStringLiteral("admin.revenue.recentOrders"));
}

void AdminClientFacade::requestChargerStatusSummary()
{
    sendAnalytics(QStringLiteral("admin.charger.statusSummary"));
}

}  // namespace ncs
