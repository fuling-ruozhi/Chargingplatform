#pragma once

#include "model/service_result.h"
#include "model/station.h"
#include "model/station_detail.h"

namespace ncs {

class StationRepository;
class ReviewRepository;

class StationService
{
public:
    explicit StationService(StationRepository &repository,
                            ReviewRepository *reviewRepository = nullptr);
    ServiceResult<QVector<Station>> list(double longitude = 0.0,
                                         double latitude = 0.0,
                                         bool hasOrigin = false) const;
    ServiceResult<StationDetail> detail(qint64 stationId, double longitude = 0.0,
                                        double latitude = 0.0,
                                        bool hasOrigin = false) const;
    ServiceResult<QVector<StationRecommendation>> recommend(
        qint64 userId, double longitude, double latitude, bool hasOrigin,
        double currentSocPercent) const;

private:
    StationRepository &repository_;
    ReviewRepository *reviewRepository_ = nullptr;
};

}
