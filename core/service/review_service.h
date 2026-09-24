#pragma once

#include "model/review.h"
#include "model/service_result.h"

namespace ncs {

class ChargeRepository;
class DatabaseManager;
class ReviewRepository;

class ReviewService
{
public:
    ReviewService(DatabaseManager &database, ChargeRepository &chargeRepository,
                  ReviewRepository &reviewRepository);

    ServiceResult<Review> submitReview(qint64 currentUserId, qint64 orderId,
                                       int environmentScore, int queueScore,
                                       int equipmentScore, int parkingScore);
    ServiceResult<Review> reviewForOrder(qint64 currentUserId, qint64 orderId) const;

private:
    DatabaseManager &database_;
    ChargeRepository &chargeRepository_;
    ReviewRepository &reviewRepository_;
};

}
