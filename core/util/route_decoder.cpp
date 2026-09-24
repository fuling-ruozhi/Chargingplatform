#include "util/route_decoder.h"

namespace ncs {

QVector<RoutePoint> decodeTencentPolyline(const QJsonArray &coors)
{
    QVector<RoutePoint> points;
    const int n = coors.size() - (coors.size() % 2);
    if (n < 2) return points;

    QVector<double> values;
    values.reserve(n);
    for (int i = 0; i < n; ++i) {
        values.append(coors.at(i).toDouble());
    }
    // 官方解码：从下标 2 起，绝对值 = 前一对的绝对值 + 当前增量 / 1e6。
    // values[i-2] 在前一轮已变为绝对值，链式累加成立。
    for (int i = 2; i < n; ++i) {
        values[i] = values[i - 2] + values[i] / 1e6;
    }
    points.reserve(n / 2);
    for (int i = 0; i < n; i += 2) {
        points.append({values.at(i), values.at(i + 1)});
    }
    return points;
}

QVector<RoutePoint> decodeOsrmPolyline6(const QString &encoded)
{
    QVector<RoutePoint> points;
    int lat = 0;
    int lng = 0;
    int index = 0;
    const int length = encoded.size();
    while (index < length) {
        auto decodeValue = [&encoded, &index, length]() -> int {
            int result = 0;
            int shift = 0;
            while (index < length) {
                const int byte = encoded.at(index).unicode() - 63;
                ++index;
                result |= (byte & 0x1f) << shift;
                shift += 5;
                if (byte < 0x20) break;
            }
            const bool negative = (result & 1) != 0;
            return negative ? ~(result >> 1) : (result >> 1);
        };
        lat += decodeValue();
        lng += decodeValue();
        points.append({lat / 1e6, lng / 1e6});
    }
    return points;
}

}  // namespace ncs
