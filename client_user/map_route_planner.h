#pragma once

#include "util/route_decoder.h"

#include <QString>
#include <QVector>

#include <functional>

class QNetworkAccessManager;
class QObject;

namespace ncs {

// 算路结果（与具体来源无关）
struct RoutePlan
{
    bool ok = false;               // 是否拿到路线
    bool quotaExhausted = false;   // 失败且原因是接口日配额用完
    QString label;                 // 成功：来源标签（驾车/步行参考/备用路线）
    QString message;               // 失败原因文案
    QVector<RoutePoint> points;    // 成功：解码后的坐标序列
    double distanceMeters = 0.0;
    double durationMinutes = 0.0;
};

// 三种算路源（链式降级用；均有独立会话缓存，同起→终点只发一次真实请求）：
// 1) 腾讯 WebService 驾车路线规划（语义最正，含路况 ETA；有每日免费配额）
// 2) 腾讯 WebService 步行路线规划（同一 key 的配额池与驾车独立）
// 3) OSRM 公共驾车路网（无需 key、无配额；海外服务，几何为真实道路）
// manager / context 必须存活到回调触发；超时/网络失败经 message 返回。
void requestTencentDriving(QNetworkAccessManager *manager, QObject *context,
                           double fromLat, double fromLng, double toLat,
                           double toLng, const QString &mapKey,
                           const std::function<void(const RoutePlan &)> &done);
void requestTencentWalking(QNetworkAccessManager *manager, QObject *context,
                           double fromLat, double fromLng, double toLat,
                           double toLng, const QString &mapKey,
                           const std::function<void(const RoutePlan &)> &done);
void requestOsrmDriving(QNetworkAccessManager *manager, QObject *context,
                        double fromLat, double fromLng, double toLat,
                        double toLng,
                        const std::function<void(const RoutePlan &)> &done);

}  // namespace ncs
