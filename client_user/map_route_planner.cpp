#include "map_route_planner.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTimer>
#include <QUrl>

namespace ncs {

namespace {

constexpr int kTencentTimeoutMs = 10000;
constexpr int kOsrmTimeoutMs = 12000;

struct CacheEntry
{
    RoutePlan plan;
};

QHash<QString, CacheEntry> &routeCache()
{
    static QHash<QString, CacheEntry> cache;
    return cache;
}

QString cacheKey(const QString &provider, double fromLat, double fromLng,
                 double toLat, double toLng)
{
    return QStringLiteral("%1|%2,%3|%4,%5")
        .arg(provider)
        .arg(fromLat, 0, 'f', 6)
        .arg(fromLng, 0, 'f', 6)
        .arg(toLat, 0, 'f', 6)
        .arg(toLng, 0, 'f', 6);
}

bool isQuotaMessage(const QString &message)
{
    return message.contains(QStringLiteral("上限"))
        || message.contains(QStringLiteral("配额"));
}

RoutePlan planFromCache(const QString &key)
{
    RoutePlan empty;
    const auto it = routeCache().constFind(key);
    return it != routeCache().constEnd() ? it->plan : empty;
}

// GET 一个 JSON 接口；超时（abort）或网络错误经 error 回调，成功回传文档。
void getJson(QNetworkAccessManager *manager, QObject *context, const QUrl &url,
             int timeoutMs,
             const std::function<void(const QJsonDocument &,
                                      const QString &error)> &done)
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "ncs_user/1.0 (embedded-route)");
    QNetworkReply *reply = manager->get(request);
    QTimer *timeout = new QTimer(context);
    timeout->setSingleShot(true);
    // reply 由 QNetworkAccessManager 持有，manager 可能先于 context 销毁；
    // 用 QPointer 守卫，避免定时器触发时访问已释放的悬空指针。
    const QPointer<QNetworkReply> replyGuard(reply);
    QObject::connect(timeout, &QTimer::timeout, context,
                     [replyGuard] {
                         if (replyGuard) replyGuard->abort();
                     });
    timeout->start(timeoutMs);
    QObject::connect(reply, &QNetworkReply::finished, context,
                     [reply, timeout, done] {
                         timeout->stop();
                         timeout->deleteLater();
                         reply->deleteLater();
                         if (reply->error() != QNetworkReply::NoError) {
                             const QString error =
                                 reply->error()
                                         == QNetworkReply::OperationCanceledError
                                 ? QStringLiteral("请求超时")
                                 : reply->errorString();
                             done(QJsonDocument(), error);
                             return;
                         }
                         done(QJsonDocument::fromJson(reply->readAll()),
                              QString());
                     });
}

// 腾讯 direction（驾车/步行共用解析：status + routes[0]）
void requestTencentDirection(const QString &mode, const QString &label,
                             QNetworkAccessManager *manager,
                             QObject *context, double fromLat, double fromLng,
                             double toLat, double toLng, const QString &mapKey,
                             const std::function<void(const RoutePlan &)> &done)
{
    const QString key =
        cacheKey(QStringLiteral("tencent-%1").arg(mode), fromLat, fromLng,
                 toLat, toLng);
    const RoutePlan cached = planFromCache(key);
    if (cached.ok) {
        done(cached);
        return;
    }
    const QUrl url(QStringLiteral(
        "https://apis.map.qq.com/ws/direction/v1/%1/?from=%2,%3"
        "&to=%4,%5&output=json&key=%6")
                       .arg(mode)
                       .arg(fromLat, 0, 'f', 6)
                       .arg(fromLng, 0, 'f', 6)
                       .arg(toLat, 0, 'f', 6)
                       .arg(toLng, 0, 'f', 6)
                       .arg(mapKey));
    getJson(manager, context, url, kTencentTimeoutMs,
            [key, label, done](const QJsonDocument &document,
                               const QString &error) {
                RoutePlan plan;
                if (!error.isEmpty()) {
                    plan.message = error;
                    done(plan);
                    return;
                }
                const QJsonObject root = document.object();
                if (root.value(QStringLiteral("status")).toInt() != 0) {
                    plan.message =
                        root.value(QStringLiteral("message")).toString();
                    plan.quotaExhausted = isQuotaMessage(plan.message);
                    done(plan);
                    return;
                }
                const QJsonArray routes =
                    root.value(QStringLiteral("result"))
                        .toObject()
                        .value(QStringLiteral("routes"))
                        .toArray();
                if (routes.isEmpty()) {
                    plan.message =
                        QStringLiteral("路线服务返回空结果（无可用路线）");
                    done(plan);
                    return;
                }
                const QJsonObject route = routes.first().toObject();
                plan.label = label;
                plan.distanceMeters =
                    route.value(QStringLiteral("distance")).toDouble();
                plan.durationMinutes =
                    route.value(QStringLiteral("duration")).toDouble();
                plan.points = decodeTencentPolyline(
                    route.value(QStringLiteral("polyline")).toArray());
                plan.ok = true;
                routeCache().insert(key, {plan});
                done(plan);
            });
}

}  // namespace

void requestTencentDriving(QNetworkAccessManager *manager, QObject *context,
                           double fromLat, double fromLng, double toLat,
                           double toLng, const QString &mapKey,
                           const std::function<void(const RoutePlan &)> &done)
{
    requestTencentDirection(QStringLiteral("driving"),
                            QStringLiteral("驾车"), manager, context, fromLat,
                            fromLng, toLat, toLng, mapKey, done);
}

void requestTencentWalking(QNetworkAccessManager *manager, QObject *context,
                           double fromLat, double fromLng, double toLat,
                           double toLng, const QString &mapKey,
                           const std::function<void(const RoutePlan &)> &done)
{
    requestTencentDirection(QStringLiteral("walking"),
                            QStringLiteral("步行参考"), manager, context,
                            fromLat, fromLng, toLat, toLng, mapKey, done);
}

void requestOsrmDriving(QNetworkAccessManager *manager, QObject *context,
                        double fromLat, double fromLng, double toLat,
                        double toLng,
                        const std::function<void(const RoutePlan &)> &done)
{
    const QString key = cacheKey(QStringLiteral("osrm"), fromLat, fromLng,
                                 toLat, toLng);
    const RoutePlan cached = planFromCache(key);
    if (cached.ok) {
        done(cached);
        return;
    }
    // OSRM 坐标顺序为 经度,纬度
    const QUrl url(QStringLiteral(
        "https://router.project-osrm.org/route/v1/driving/%1,%2;%3,%4"
        "?overview=full&geometries=polyline6&steps=false")
                       .arg(fromLng, 0, 'f', 6)
                       .arg(fromLat, 0, 'f', 6)
                       .arg(toLng, 0, 'f', 6)
                       .arg(toLat, 0, 'f', 6));
    getJson(manager, context, url, kOsrmTimeoutMs,
            [key, done](const QJsonDocument &document, const QString &error) {
                RoutePlan plan;
                if (!error.isEmpty()) {
                    plan.message = error;
                    done(plan);
                    return;
                }
                const QJsonObject root = document.object();
                if (root.value(QStringLiteral("code")).toString()
                    != QStringLiteral("Ok")) {
                    plan.message =
                        QStringLiteral("路网服务返回错误：%1")
                            .arg(root.value(QStringLiteral("code")).toString());
                    done(plan);
                    return;
                }
                const QJsonArray routes =
                    root.value(QStringLiteral("routes")).toArray();
                if (routes.isEmpty()) {
                    plan.message =
                        QStringLiteral("备用路网返回空结果（无可用路线）");
                    done(plan);
                    return;
                }
                const QJsonObject route = routes.first().toObject();
                plan.label = QStringLiteral("备用路线");
                plan.distanceMeters =
                    route.value(QStringLiteral("distance")).toDouble();
                // OSRM duration 单位为秒
                plan.durationMinutes =
                    route.value(QStringLiteral("duration")).toDouble() / 60.0;
                plan.points = decodeOsrmPolyline6(
                    route.value(QStringLiteral("geometry")).toString());
                plan.ok = true;
                routeCache().insert(key, {plan});
                done(plan);
            });
}

}  // namespace ncs
