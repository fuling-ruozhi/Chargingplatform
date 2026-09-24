#include "util/route_decoder.h"

#include <QJsonArray>

#include <cmath>

namespace {

bool near(double a, double b)
{
    return std::fabs(a - b) < 1e-6;
}

}  // namespace

// UC-U-04 增强：腾讯 WebService polyline 差分解码（官方文档示例 + 真实接口样例）。
int main()
{
    // 官方文档示例：coors=[50.243916,127.496637,-345,-1828,19867,-26154]
    {
        const QJsonArray coors = {50.243916, 127.496637, -345, -1828,
                                  19867, -26154};
        const auto points = ncs::decodeTencentPolyline(coors);
        if (points.size() != 3) return 1;
        if (!near(points.at(0).lat, 50.243916)
            || !near(points.at(0).lng, 127.496637)) return 2;
        if (!near(points.at(1).lat, 50.243571)
            || !near(points.at(1).lng, 127.494809)) return 3;
        if (!near(points.at(2).lat, 50.263438)
            || !near(points.at(2).lng, 127.468655)) return 4;
    }
    // 2026-09 真实接口实测样例（ws/direction/v1/driving 返回头 10 项）
    {
        const QJsonArray coors = {39.984094, 116.307958, 14, 112, 0, 0,
                                  -1191, -25, -151, 3};
        const auto points = ncs::decodeTencentPolyline(coors);
        if (points.size() != 5) return 5;
        if (!near(points.at(0).lat, 39.984094)
            || !near(points.at(0).lng, 116.307958)) return 6;
        if (!near(points.at(1).lat, 39.984108)
            || !near(points.at(1).lng, 116.308070)) return 7;
        // 0,0 间隔产生与上一坐标重合的中间点（路网几何点）
        if (!near(points.at(2).lat, 39.984108)
            || !near(points.at(2).lng, 116.308070)) return 8;
        if (!near(points.at(3).lat, 39.982917)
            || !near(points.at(3).lng, 116.308045)) return 9;
        if (!near(points.at(4).lat, 39.982766)
            || !near(points.at(4).lng, 116.308048)) return 10;
    }
    // 空数组与奇数长度（末位忽略）
    {
        if (!ncs::decodeTencentPolyline(QJsonArray()).isEmpty()) return 11;
        const QJsonArray odd = {39.9, 116.3, 1.0};
        const auto points = ncs::decodeTencentPolyline(odd);
        if (points.size() != 1) return 12;
    }
    // OSRM polyline6（合成极小样例）：??AC → (0,0),(1e-6,2e-6)
    {
        const auto points = ncs::decodeOsrmPolyline6(QStringLiteral("??AC"));
        if (points.size() != 2) return 20;
        if (!near(points.at(0).lat, 0.0) || !near(points.at(0).lng, 0.0))
            return 21;
        if (!near(points.at(1).lat, 1e-6) || !near(points.at(1).lng, 2e-6))
            return 22;
    }
    // OSRM polyline6（真实接口实测几何串，2026-09 抓取）
    {
        const QString geometry = R"(mwlgkAu~zy|E}BwoBeAku@gAquAc@yi@kAobAYc\Yi]w@s~@}CeqDkC}zBKaJGkGSyYuBmjBm@mk@oEqxD{@eo@sHokEwLgcH]ySuFutDyBicEl@qq@?u_@e@u]Wgu@Gwf@yAesByZ`AtCzqBp@vh@J~NfBvrBfIbc@zBtaExHdiEvLrjHhGnaEDjB|@jm@DdBrEzuDf@ha@JdI|BhjB^nZXhWbKreK\n]\`\Xde@Zli@x@vuAx@ft@nFf~F\`_@fBdxAkD~j@mFdn@nC|kCFzFp`@U_@gWmAikBMcLq@o_A~EKpb@_AfP_@hEKEmFQoZ)";
        const auto points = ncs::decodeOsrmPolyline6(geometry);
        if (points.size() != 73) return 23;
        if (!near(points.at(0).lat, 39.984007)
            || !near(points.at(0).lng, 116.307963)) return 24;
        if (!near(points.at(1).lat, 39.98407)
            || !near(points.at(1).lng, 116.309767)) return 25;
        if (!near(points.at(2).lat, 39.984105)
            || !near(points.at(2).lng, 116.310637)) return 26;
        const auto last = points.constLast();
        if (!near(last.lat, 39.982693) || !near(last.lng, 116.303899))
            return 27;
    }
    return 0;
}
