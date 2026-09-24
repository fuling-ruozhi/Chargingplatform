#include "review_service.h"

#include "database/database_manager.h"
#include "repository/charge_repository.h"
#include "repository/review_repository.h"

namespace ncs {
namespace {

template<typename T>
ServiceResult<T> databaseFailure(const QString &error)
{
    return ServiceResult<T>::fail(BusinessErrorCode::DatabaseError, error);
}

bool validScore(int score)
{
    return score >= 1 && score <= 5;
}

}

ReviewService::ReviewService(DatabaseManager &database, ChargeRepository &chargeRepository,
                             ReviewRepository &reviewRepository)
    : database_(database), chargeRepository_(chargeRepository),
      reviewRepository_(reviewRepository)
{
}

ServiceResult<Review> ReviewService::submitReview(qint64 currentUserId, qint64 orderId,
                                                  int environmentScore, int queueScore,
                                                  int equipmentScore, int parkingScore)
{
    if (currentUserId <= 0 || orderId <= 0) {
        return ServiceResult<Review>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("评价参数无效"));
    }
    if (!validScore(environmentScore) || !validScore(queueScore)
        || !validScore(equipmentScore) || !validScore(parkingScore)) {
        return ServiceResult<Review>::fail(
            BusinessErrorCode::InvalidReviewScore, QStringLiteral("评价分数必须在1到5之间"));
    }

    bool userFound = false;
    QString error;
    if (!chargeRepository_.userExists(currentUserId, &userFound, &error)) {
        return databaseFailure<Review>(error);
    }
    if (!userFound) {
        return ServiceResult<Review>::fail(
            BusinessErrorCode::UserNotFound, QStringLiteral("用户不存在"));
    }

    ChargingRecord order;
    bool orderFound = false;
    if (!chargeRepository_.findById(orderId, &order, &orderFound, &error)) {
        return databaseFailure<Review>(error);
    }
    if (!orderFound) {
        return ServiceResult<Review>::fail(
            BusinessErrorCode::OrderNotFound, QStringLiteral("订单不存在"));
    }
    if (order.userId != currentUserId) {
        return ServiceResult<Review>::fail(
            BusinessErrorCode::InvalidOrderOwner, QStringLiteral("无权评价该订单"));
    }
    if (order.status != ChargingOrderStatus::Completed) {
        return ServiceResult<Review>::fail(
            BusinessErrorCode::InvalidOrderState, QStringLiteral("只有已结算订单可以评价"));
    }

    Review review;
    review.orderId = order.id;
    review.userId = order.userId;
    review.stationId = order.stationId;
    review.chargerId = order.chargerId;
    review.environmentScore = environmentScore;
    review.queueScore = queueScore;
    review.equipmentScore = equipmentScore;
    review.parkingScore = parkingScore;
    review.overallScore = (environmentScore + queueScore + equipmentScore + parkingScore) / 4.0;

    if (!database_.transaction()) return databaseFailure<Review>(database_.lastError());
    bool duplicateOrder = false;
    if (!reviewRepository_.createReview(review, &review, &error, &duplicateOrder)) {
        database_.rollback();
        if (duplicateOrder) {
            return ServiceResult<Review>::fail(
                BusinessErrorCode::ReviewAlreadyExists, QStringLiteral("该订单已经评价"));
        }
        return databaseFailure<Review>(error);
    }
    if (!database_.commit()) {
        const QString commitError = database_.lastError();
        database_.rollback();
        return databaseFailure<Review>(commitError);
    }
    return ServiceResult<Review>::ok(review);
}

ServiceResult<Review> ReviewService::reviewForOrder(qint64 currentUserId,
                                                    qint64 orderId) const
{
    if (currentUserId <= 0 || orderId <= 0) {
        return ServiceResult<Review>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("订单编号无效"));
    }
    ChargingRecord order;
    bool orderFound = false;
    QString error;
    if (!chargeRepository_.findById(orderId, &order, &orderFound, &error)) {
        return databaseFailure<Review>(error);
    }
    if (!orderFound) {
        return ServiceResult<Review>::fail(
            BusinessErrorCode::OrderNotFound, QStringLiteral("订单不存在"));
    }
    if (order.userId != currentUserId) {
        return ServiceResult<Review>::fail(
            BusinessErrorCode::InvalidOrderOwner, QStringLiteral("无权查看该订单评价"));
    }
    Review review;
    bool found = false;
    if (!reviewRepository_.findByOrderId(orderId, &review, &found, &error)) {
        return databaseFailure<Review>(error);
    }
    return ServiceResult<Review>::ok(found ? review : Review());
}

}
