#include "station_rating_insight.h"

namespace ncs {

QVector<StationRatingInsight> stationRatingInsights(const StationRatingSummary &summary)
{
    if (summary.reviewCount < 3)
        return {{QStringLiteral("sample"), QStringLiteral("info"), QStringLiteral("评价样本不足")}};
    QVector<StationRatingInsight> result;
    const auto add = [&result](const QString &type, double score, const QString &message) {
        if (score < 3.0)
            result.append({type, QStringLiteral("warning"), message});
    };
    add(QStringLiteral("environment"), summary.environmentAverage,
        QStringLiteral("环境体验偏低，建议安排现场巡检"));
    add(QStringLiteral("queue"), summary.queueAverage,
        QStringLiteral("排队体验偏低，建议关注高峰期容量"));
    add(QStringLiteral("equipment"), summary.equipmentAverage,
        QStringLiteral("设备体验偏低，建议检查充电设备状态"));
    add(QStringLiteral("parking"), summary.parkingAverage,
        QStringLiteral("停车体验偏低，建议检查停车资源"));
    if (result.isEmpty())
        result.append({QStringLiteral("normal"), QStringLiteral("info"), QStringLiteral("评价状态正常")});
    return result;
}

}
