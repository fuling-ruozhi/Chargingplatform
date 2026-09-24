#include "user_service.h"

#include "database/database_manager.h"
#include "model/vehicle_profile.h"
#include "repository/user_repository.h"
#include "util/password_hasher.h"
#include "util/logger.h"
#include "util/money.h"

#include <QRandomGenerator>
#include <QRegularExpression>

#include <cmath>
#include <utility>

namespace ncs {

UserService::UserService(DatabaseManager &database, UserRepository &repository,
                         std::function<qint64()> clock)
    : db_(database), repo_(repository), authRateLimiter_(std::move(clock))
{
}

bool UserService::validPhone(const QString &phone)
{
    static const QRegularExpression expression(QStringLiteral("^1[0-9]{10}$"));
    return expression.match(phone).hasMatch();
}

bool UserService::validUsername(const QString &username)
{
    static const QRegularExpression expression(QStringLiteral("^[A-Za-z0-9_]{3,20}$"));
    return expression.match(username).hasMatch();
}

QString UserService::maskedPhone(const QString &phone)
{
    return validPhone(phone)
        ? phone.left(3) + QStringLiteral("****") + phone.right(4)
        : QString();
}

ServiceResult<OtpChallenge> UserService::requestOtp(const QString &rawPhone)
{
    const QString phone = rawPhone.trimmed();
    if (!validPhone(phone)) {
        Logger::warning(QStringLiteral("user-auth"),
                        QStringLiteral("OTP request rejected: invalid phone format"));
        return ServiceResult<OtpChallenge>::fail(
            BusinessErrorCode::InvalidPhone, QStringLiteral("请输入11位、1开头的手机号"));
    }
    if (!authRateLimiter_.allowOtpRequest(phone)) {
        return ServiceResult<OtpChallenge>::fail(
            BusinessErrorCode::TooFrequent, QStringLiteral("验证码请求过于频繁，请稍后再试"));
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const auto existing = otpStates_.constFind(phone);
    if (existing != otpStates_.constEnd() && existing->resendAt > now) {
        return ServiceResult<OtpChallenge>::fail(
            BusinessErrorCode::OtpCooldown, QStringLiteral("验证码发送过于频繁，请稍后再试"));
    }

    const int numericCode = QRandomGenerator::system()->bounded(1000000);
    OtpState state;
    state.code = QString::number(numericCode).rightJustified(6, QLatin1Char('0'));
    state.expiresAt = now.addSecs(300);
    state.resendAt = now.addSecs(60);
    otpStates_.insert(phone, state);
    return ServiceResult<OtpChallenge>::ok({state.code, 60, 300});
}

ServiceResult<User> UserService::loginWithOtp(const QString &rawPhone,
                                              const QString &rawCode)
{
    const QString phone = rawPhone.trimmed();
    const QString code = rawCode.trimmed();
    if (!validPhone(phone)) {
        Logger::warning(QStringLiteral("user-auth"),
                        QStringLiteral("Login rejected: invalid phone format"));
        return ServiceResult<User>::fail(
            BusinessErrorCode::InvalidPhone, QStringLiteral("请输入11位、1开头的手机号"));
    }
    if (!authRateLimiter_.allowLogin(phone)) {
        return ServiceResult<User>::fail(
            BusinessErrorCode::TooFrequent, QStringLiteral("登录尝试过于频繁，请稍后再试"));
    }
    auto challenge = otpStates_.find(phone);
    if (challenge == otpStates_.end()
        || challenge->expiresAt <= QDateTime::currentDateTimeUtc()) {
        Logger::warning(QStringLiteral("user-auth"),
                        QStringLiteral("Login rejected: OTP expired"));
        otpStates_.remove(phone);
        return ServiceResult<User>::fail(
            BusinessErrorCode::VerificationCodeExpired,
            QStringLiteral("验证码已过期，请重新获取"));
    }
    if (code.size() != 6 || code != challenge->code) {
        Logger::warning(QStringLiteral("user-auth"),
                        QStringLiteral("Login rejected: invalid OTP"));
        ++challenge->failedAttempts;
        if (challenge->failedAttempts >= 5) otpStates_.erase(challenge);
        if (authRateLimiter_.recordLoginFailure(phone)) {
            return ServiceResult<User>::fail(
                BusinessErrorCode::TooFrequent,
                QStringLiteral("登录尝试过于频繁，请稍后再试"));
        }
        return ServiceResult<User>::fail(
            BusinessErrorCode::InvalidVerificationCode,
            QStringLiteral("验证码不正确"));
    }

    User user;
    bool found = false;
    QString error;
    if (!repo_.findByPhone(phone, &user, &found, &error)) {
        return ServiceResult<User>::fail(BusinessErrorCode::DatabaseError, error);
    }
    if (found && user.status == 0) {
        Logger::warning(QStringLiteral("user-auth"),
                        QStringLiteral("Login rejected: account frozen"));
        return ServiceResult<User>::fail(
            BusinessErrorCode::UserFrozen, QStringLiteral("账号已冻结，请联系管理员"));
    }
    if (!found) {
        if (!db_.transaction()) {
            return ServiceResult<User>::fail(
                BusinessErrorCode::DatabaseError, db_.lastError());
        }
        const QString nickname = QStringLiteral("用户") + phone.right(4);
        if (!repo_.createByPhone(phone, nickname, &user, &error) || !db_.commit()) {
            db_.rollback();
            return ServiceResult<User>::fail(
                BusinessErrorCode::DatabaseError,
                error.isEmpty() ? db_.lastError() : error);
        }
    }
    otpStates_.remove(phone);
    authRateLimiter_.recordLoginSuccess(phone);
    Logger::info(QStringLiteral("user-auth"), QStringLiteral("Login succeeded"));
    return ServiceResult<User>::ok(user);
}

ServiceResult<User> UserService::repositoryUser(qint64 userId) const
{
    User user;
    bool found = false;
    QString error;
    if (!repo_.findById(userId, &user, &found, &error)) {
        return ServiceResult<User>::fail(BusinessErrorCode::DatabaseError, error);
    }
    if (!found) {
        return ServiceResult<User>::fail(
            BusinessErrorCode::UserNotFound, QStringLiteral("用户不存在"));
    }
    return ServiceResult<User>::ok(user);
}

ServiceResult<User> UserService::profile(qint64 userId) const
{
    return repositoryUser(userId);
}

ServiceResult<User> UserService::updateNickname(qint64 userId,
                                                const QString &rawNickname)
{
    const QString nickname = rawNickname.trimmed();
    if (nickname.isEmpty() || nickname.size() > 20) {
        return ServiceResult<User>::fail(
            BusinessErrorCode::InvalidProfile, QStringLiteral("昵称长度应为1至20个字符"));
    }
    QString error;
    if (!repo_.updateNickname(userId, nickname, &error)) {
        return ServiceResult<User>::fail(BusinessErrorCode::DatabaseError, error);
    }
    return repositoryUser(userId);
}

ServiceResult<User> UserService::updateAvatar(qint64 userId,
                                              const QString &avatarPath)
{
    const bool valid = avatarPath.isEmpty()
        || (avatarPath.startsWith(QStringLiteral("avatars/"))
            && !avatarPath.contains(QStringLiteral(".."))
            && !avatarPath.contains(QLatin1Char(':'))
            && !avatarPath.contains(QLatin1Char('\\')));
    if (!valid || avatarPath.size() > 200) {
        return ServiceResult<User>::fail(
            BusinessErrorCode::InvalidProfile, QStringLiteral("头像路径无效"));
    }
    QString error;
    if (!repo_.updateAvatar(userId, avatarPath, &error)) {
        return ServiceResult<User>::fail(BusinessErrorCode::DatabaseError, error);
    }
    return repositoryUser(userId);
}

ServiceResult<RechargeResult> UserService::recharge(qint64 userId, double amount)
{
    const qint64 cents = Money::toCents(amount);
    if (!std::isfinite(amount) || cents < 1 || cents > 1000000
        || !Money::hasCentPrecision(amount)) {
        return ServiceResult<RechargeResult>::fail(
            BusinessErrorCode::InvalidRechargeAmount,
            QStringLiteral("充值金额应为0.01至10000元，最多两位小数"));
    }
    if (!db_.transaction()) {
        return ServiceResult<RechargeResult>::fail(
            BusinessErrorCode::DatabaseError, db_.lastError());
    }
    const auto current = repositoryUser(userId);
    const double amountValue = Money::fromCents(cents);
    const double after = current.success
        ? Money::round(current.value.balance + amountValue) : 0.0;
    QString error;
    qint64 logId = 0;
    if (!current.success
        || !repo_.updateBalance(userId, after, &error)
        || !repo_.createRechargeLog(userId, amountValue, current.value.balance,
                                    after, &logId, &error)
        || !db_.commit()) {
        db_.rollback();
        return ServiceResult<RechargeResult>::fail(
            current.success ? BusinessErrorCode::DatabaseError : current.code,
            current.success ? (error.isEmpty() ? db_.lastError() : error)
                            : current.message);
    }
    return ServiceResult<RechargeResult>::ok(
        {logId, amountValue, current.value.balance, after});
}

ServiceResult<VehicleProfile> UserService::vehicleProfile(qint64 userId) const
{
    VehicleProfile profile;
    bool found = false;
    QString error;
    if (!repo_.findVehicleProfile(userId, &profile, &found, &error)) {
        return ServiceResult<VehicleProfile>::fail(BusinessErrorCode::DatabaseError, error);
    }
    if (found) return ServiceResult<VehicleProfile>::ok(profile);

    const auto user = repositoryUser(userId);
    if (!user.success) {
        return ServiceResult<VehicleProfile>::fail(user.code, user.message);
    }
    profile.userId = userId;
    return ServiceResult<VehicleProfile>::ok(profile);
}

ServiceResult<VehicleProfile> UserService::updateVehicleProfile(
    qint64 userId, const VehicleProfile &raw)
{
    const auto user = repositoryUser(userId);
    if (!user.success) {
        return ServiceResult<VehicleProfile>::fail(user.code, user.message);
    }
    VehicleProfile profile = raw;
    if (!std::isfinite(profile.batteryCapacityKwh) || profile.batteryCapacityKwh < 1.0
        || profile.batteryCapacityKwh > 200.0) {
        return ServiceResult<VehicleProfile>::fail(
            BusinessErrorCode::InvalidProfile,
            QStringLiteral("电池容量应在1至200 kWh之间"));
    }
    if (!std::isfinite(profile.targetSoc) || profile.targetSoc < 10.0
        || profile.targetSoc > 100.0) {
        return ServiceResult<VehicleProfile>::fail(
            BusinessErrorCode::InvalidProfile,
            QStringLiteral("目标电量应在10%至100%之间"));
    }
    if (!std::isfinite(profile.minBalanceReserve) || profile.minBalanceReserve < 0.0
        || profile.minBalanceReserve > 100000.0
        || !Money::hasCentPrecision(profile.minBalanceReserve)) {
        return ServiceResult<VehicleProfile>::fail(
            BusinessErrorCode::InvalidProfile, QStringLiteral("余额保留值无效"));
    }
    static const QRegularExpression timePattern(
        QStringLiteral("^([01][0-9]|2[0-3]):[0-5][0-9]$"));
    if (!timePattern.match(profile.usualLeaveTime).hasMatch()) {
        return ServiceResult<VehicleProfile>::fail(
            BusinessErrorCode::InvalidProfile,
            QStringLiteral("常用离开时间格式应为HH:mm"));
    }
    if (profile.chargeMode < static_cast<int>(ChargeMode::Balanced)
        || profile.chargeMode > static_cast<int>(ChargeMode::Fast)) {
        return ServiceResult<VehicleProfile>::fail(
            BusinessErrorCode::InvalidProfile, QStringLiteral("充电模式无效"));
    }
    profile.userId = userId;
    QString error;
    if (!repo_.upsertVehicleProfile(profile, &error)) {
        return ServiceResult<VehicleProfile>::fail(
            BusinessErrorCode::DatabaseError, error);
    }
    return ServiceResult<VehicleProfile>::ok(profile);
}

ServiceResult<User> UserService::registerUser(const QString &rawUsername,
                                              const QString &password)
{
    const QString username = rawUsername.trimmed();
    if (!validUsername(username) || password.size() < 6 || password.size() > 64) {
        return ServiceResult<User>::fail(
            BusinessErrorCode::InvalidArgument,
            QStringLiteral("用户名需为3-20位字母/数字/下划线，密码需为6-64位"));
    }
    User existing;
    QString hash;
    QString salt;
    QString error;
    bool found = false;
    if (!repo_.find(username, &existing, &hash, &salt, &found, &error)) {
        return ServiceResult<User>::fail(BusinessErrorCode::DatabaseError, error);
    }
    if (found) {
        return ServiceResult<User>::fail(
            BusinessErrorCode::UsernameExists, QStringLiteral("用户名已存在"));
    }
    salt = PasswordHasher::makeSalt();
    if (!db_.transaction()) {
        return ServiceResult<User>::fail(BusinessErrorCode::DatabaseError, db_.lastError());
    }
    User user;
    if (!repo_.create(username, PasswordHasher::hash(password, salt), salt,
                      username, &user, &error)
        || !db_.commit()) {
        db_.rollback();
        return ServiceResult<User>::fail(
            BusinessErrorCode::DatabaseError,
            error.isEmpty() ? db_.lastError() : error);
    }
    return ServiceResult<User>::ok(user);
}

ServiceResult<User> UserService::login(const QString &rawUsername,
                                       const QString &password) const
{
    const QString username = rawUsername.trimmed();
    User user;
    QString hash;
    QString salt;
    QString error;
    bool found = false;
    if (!repo_.find(username, &user, &hash, &salt, &found, &error)) {
        return ServiceResult<User>::fail(BusinessErrorCode::DatabaseError, error);
    }
    if (!found || !PasswordHasher::verify(password, salt, hash)) {
        return ServiceResult<User>::fail(
            BusinessErrorCode::InvalidCredentials, QStringLiteral("用户名或密码错误"));
    }
    if (user.status == 0) {
        return ServiceResult<User>::fail(
            BusinessErrorCode::UserFrozen, QStringLiteral("账号已冻结，请联系管理员"));
    }
    return ServiceResult<User>::ok(user);
}

}
