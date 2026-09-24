#include "review_json.h"

namespace ncs {

QJsonObject reviewJson(const Review &review)
{
    return {{QStringLiteral("review_id"), review.id},
            {QStringLiteral("order_id"), review.orderId},
            {QStringLiteral("user_id"), review.userId},
            {QStringLiteral("station_id"), review.stationId},
            {QStringLiteral("charger_id"), review.chargerId},
            {QStringLiteral("environment_score"), review.environmentScore},
            {QStringLiteral("queue_score"), review.queueScore},
            {QStringLiteral("equipment_score"), review.equipmentScore},
            {QStringLiteral("parking_score"), review.parkingScore},
            {QStringLiteral("overall_score"), review.overallScore},
            {QStringLiteral("created_at"), review.createdAt}};
}

QJsonObject ratingSummaryJson(const StationRatingSummary &summary)
{
    return {{QStringLiteral("review_count"), summary.reviewCount},
            {QStringLiteral("average_score"), summary.averageScore},
            {QStringLiteral("environment_score"), summary.environmentAverage},
            {QStringLiteral("queue_score"), summary.queueAverage},
            {QStringLiteral("equipment_score"), summary.equipmentAverage},
            {QStringLiteral("parking_score"), summary.parkingAverage}};
}

}
