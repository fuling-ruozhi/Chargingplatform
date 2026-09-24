#pragma once

#include "model/review.h"

namespace ncs {

class DatabaseManager;

class ReviewRepository
{
public:
    explicit ReviewRepository(DatabaseManager &database);

    bool createReview(const Review &review, Review *created, QString *error,
                      bool *duplicateOrder = nullptr) const;
    bool findByOrderId(qint64 orderId, Review *review, bool *found,
                       QString *error) const;
    bool getStationRatingSummary(qint64 stationId, StationRatingSummary *summary,
                                 QString *error) const;

private:
    DatabaseManager &database_;
};

}
