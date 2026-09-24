#pragma once

#include "model/admin.h"
#include "model/charger.h"
#include "model/charger_status_summary.h"
#include "model/revenue_stats.h"
#include "model/service_result.h"
#include "model/station.h"
#include "model/charger.h"
#include "model/user.h"
#include "model/charging_record.h"
#include "model/load_prediction.h"

#include <QDateTime>
#include <QHash>

namespace ncs {

class AdminRepository;
class ChargerService;
class DatabaseManager;
class RevenueService;
class StationRepository;
class UserRepository;
class ChargeRepository;
class PredictionService;
class LogService;

class AdminService
{
public:
    AdminService(DatabaseManager &database, AdminRepository &repository);
    AdminService(DatabaseManager &database, AdminRepository &repository,
                 RevenueService &revenueService, ChargerService &chargerService);
    void setStationRepository(StationRepository &repository) { stationRepository_ = &repository; }
    void setUserRepository(UserRepository &repository) { userRepository_ = &repository; }
    void setChargeRepository(ChargeRepository &repository) { chargeRepository_ = &repository; }
    void setPredictionService(PredictionService &service) { predictionService_ = &service; }
    void setLogService(LogService &service) { logService_ = &service; }

    ServiceResult<Admin> login(const QString &username, const QString &password);
    ServiceResult<AdminSummary> summary(qint64 adminId) const;
    ServiceResult<RevenueSummary> revenueSummary(qint64 adminId) const;
    ServiceResult<RevenueTrend> revenueTrend(qint64 adminId, int days) const;
    ServiceResult<RecentOrders> recentOrders(qint64 adminId) const;
    ServiceResult<ChargerStatusSummary> chargerStatusSummary(qint64 adminId) const;
    ServiceResult<QVector<Charger>> chargerList(qint64 adminId,
                                                const QString &keyword,
                                                int status, qint64 stationId = -1) const;
    ServiceResult<Charger> createCharger(qint64 adminId, qint64 stationId,
                                          const QString &code, int type,
                                          double powerKw) const;
    ServiceResult<Charger> deleteCharger(qint64 adminId, qint64 chargerId) const;
    ServiceResult<Charger> markChargerFault(qint64 adminId, qint64 chargerId) const;
    ServiceResult<Charger> recoverCharger(qint64 adminId, qint64 chargerId) const;
    ServiceResult<Charger> restartCharger(qint64 adminId, qint64 chargerId) const;
    ServiceResult<QVector<Station>> stationList(qint64 adminId, const QString &keyword) const;
    ServiceResult<Station> createStation(qint64 adminId, Station station) const;
    ServiceResult<Station> updateStation(qint64 adminId, Station station) const;
    ServiceResult<Station> deleteStation(qint64 adminId, qint64 stationId) const;
    ServiceResult<QVector<Charger>> batchCreateChargers(qint64 adminId, qint64 stationId,
                                                        const QString &prefix, int count,
                                                        int type, double powerKw) const;
    ServiceResult<QVector<User>> userList(qint64 adminId, const QString &keyword) const;
    ServiceResult<User> freezeUser(qint64 adminId, qint64 userId) const;
    ServiceResult<User> unfreezeUser(qint64 adminId, qint64 userId) const;
    ServiceResult<QVector<ChargingRecord>> userOrders(qint64 adminId, qint64 userId) const;
    ServiceResult<PredictionBundle> predictionList(qint64 adminId) const;
    ServiceResult<bool> runPrediction(qint64 adminId);
    int retryAfterSeconds(const QString &username) const;
    LogService *logService() const { return logService_; }

private:
    struct LoginState {
        int failures = 0;
        QDateTime lockedUntil;
    };

    DatabaseManager &database_;
    AdminRepository &repository_;
    RevenueService *revenueService_ = nullptr;
    ChargerService *chargerService_ = nullptr;
    StationRepository *stationRepository_ = nullptr;
    UserRepository *userRepository_ = nullptr;
    ChargeRepository *chargeRepository_ = nullptr;
    PredictionService *predictionService_ = nullptr;
    LogService *logService_ = nullptr;
    QHash<QString, LoginState> loginStates_;
};

}
