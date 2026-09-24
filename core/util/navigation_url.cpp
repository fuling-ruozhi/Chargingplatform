#include "navigation_url.h"

#include <QUrlQuery>

namespace ncs {

QUrl NavigationUrl::build(double fromLongitude, double fromLatitude,
                          double toLongitude, double toLatitude,
                          const QString &destinationName)
{
    // 腾讯地图路线规划 URI（说明书 §7.4 降级方案，无需 Key）。
    // ⚠️ 腾讯 URI 的 coord 参数顺序是纬度在前、经度在后，写反会导航到错误地点。
    QUrl url(QStringLiteral("https://apis.map.qq.com/uri/v1/routeplan"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("type"), QStringLiteral("drive"));
    query.addQueryItem(QStringLiteral("from"), QStringLiteral("当前位置"));
    query.addQueryItem(QStringLiteral("fromcoord"),
                       QStringLiteral("%1,%2").arg(fromLatitude, 0, 'f', 6)
                                               .arg(fromLongitude, 0, 'f', 6));
    query.addQueryItem(QStringLiteral("to"), destinationName);
    query.addQueryItem(QStringLiteral("tocoord"),
                       QStringLiteral("%1,%2").arg(toLatitude, 0, 'f', 6)
                                               .arg(toLongitude, 0, 'f', 6));
    query.addQueryItem(QStringLiteral("policy"), QStringLiteral("0"));
    query.addQueryItem(QStringLiteral("referer"), QStringLiteral("NCS"));
    url.setQuery(query);
    return url;
}

}
