#pragma once

#include "model/service_result.h"
#include "model/user.h"
#include "model/vehicle_profile.h"
#include "service/auth_rate_limiter.h"

#include <QDateTime>
#include <QHash>

namespace ncs {

class DatabaseManager;
class UserRepository;

class UserService
{
public:
    UserService(DatabaseManager &database, UserRepository &repository,
                std::function<qint64()> clock = {});

    ServiceResult<OtpChallenge> requestOtp(const QString &phone);
    ServiceResult<User> loginWithOtp(const QString &phone, const QString &code);
    ServiceResult<User> profile(qint64 userId) const;
    ServiceResult<User> updateNickname(qint64 userId, const QString &nickname);
    ServiceResult<User> updateAvatar(qint64 userId, const QString &avatarPath);
    ServiceResult<RechargeResult> recharge(qint64 userId, double amount);
    ServiceResult<VehicleProfile> vehicleProfile(qint64 userId) const;
    ServiceResult<VehicleProfile> updateVehicleProfile(qint64 userId,
                                                       const VehicleProfile &profile);

    ServiceResult<User> registerUser(const QString &username, const QString &password);
    ServiceResult<User> login(const QString &username, const QString &password) const;

    static bool validPhone(const QString &phone);
    static bool validUsername(const QString &username);
    static QString maskedPhone(const QString &phone);

private:
    struct OtpState {
        QString code;
        QDateTime expiresAt;
        QDateTime resendAt;
        int failedAttempts = 0;
    };

    ServiceResult<User> repositoryUser(qint64 userId) const;
    DatabaseManager &db_;
    UserRepository &repo_;
    QHash<QString, OtpState> otpStates_;
    AuthRateLimiter authRateLimiter_;
};

}
