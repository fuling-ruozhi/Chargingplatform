#pragma once

#include <QJsonArray>
#include <QVector>

namespace ncs {

struct RoutePoint
{
    double lat = 0.0;
    double lng = 0.0;
};

// 解码腾讯 WebService「路线规划」接口返回的差分压缩坐标点串 polyline。
// 规则（官方文档）：元素成对出现（纬度,经度）；首点为原始坐标，
// 之后每项为相对前一对坐标的微度增量：
//   abs(i) = abs(i-2) + value(i) / 1e6
// 例：[39.915219, 116.403857, 0, 12, 40, 1180] →
//   点1 = (39.915219, 116.403857)
//   点2 = (39.915219 + 0/1e6, 116.403857 + 12/1e6)
//   点3 = (点2.lat + 40/1e6, 点2.lng + 1180/1e6)
// coors 长度必须为偶数；奇数时末位忽略；空数组返回空。
QVector<RoutePoint> decodeTencentPolyline(const QJsonArray &coors);

// 解码 OSRM(router.project-osrm.org) 返回的 polyline6 几何串
// （Google Encoded Polyline，精度 1e6；经纬度交替、增量 + 正负号折半编码）。
QVector<RoutePoint> decodeOsrmPolyline6(const QString &encoded);

}  // namespace ncs
