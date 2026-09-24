#include "admin_service.h"

#include "database/database_manager.h"
#include "repository/admin_repository.h"
#include "service/charger_service.h"
#include "service/revenue_service.h"
#include "repository/station_repository.h"
#include "repository/user_repository.h"
#include "repository/charge_repository.h"
#include "service/prediction_service.h"
#include "util/password_hasher.h"
#include "util/logger.h"
#include "service/log_service.h"
#include <cmath>

namespace ncs {

AdminService::AdminService(DatabaseManager &database, AdminRepository &repository)
    : database_(database), repository_(repository)
{
}

AdminService::AdminService(DatabaseManager &database, AdminRepository &repository,
                           RevenueService &revenueService,
                           ChargerService &chargerService)
    : database_(database), repository_(repository), revenueService_(&revenueService),
      chargerService_(&chargerService)
{
}

ServiceResult<Admin> AdminService::login(const QString &rawUsername,
                                         const QString &password)
{
    const QString username = rawUsername.trimmed();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    auto state = loginStates_.find(username);
    if (state != loginStates_.end() && state->lockedUntil > now) {
        if (logService_) { logService_->login(0, username, "LOCKED", "ACCOUNT_LOCKED", state->failures); logService_->security("ACCOUNT_LOCKED", "HIGH", username, "登录期间账号仍处于锁定状态"); }
        Logger::warning(QStringLiteral("admin-auth"),
                        QStringLiteral("Login rejected during lockout"));
        return ServiceResult<Admin>::fail(
            BusinessErrorCode::AdminLocked,
            QStringLiteral("登录失败次数过多，请稍后重试"));
    }
    if (state != loginStates_.end() && state->lockedUntil.isValid()) {
        loginStates_.erase(state);
    }

    Admin admin;
    QString hash;
    QString salt;
    QString error;
    bool found = false;
    if (!repository_.findByUsername(username, &admin, &hash, &salt, &found, &error)) {
        if (logService_) logService_->login(0, username, "FAILED", "OTHER", 0);
        return ServiceResult<Admin>::fail(BusinessErrorCode::DatabaseError, error);
    }
    if (!found || !PasswordHasher::verify(password, salt, hash)) {
        LoginState &failure = loginStates_[username];
        ++failure.failures;
        if (failure.failures >= 5) {
            failure.lockedUntil = now.addSecs(30);
            if (logService_) { logService_->login(found ? admin.id : 0, username, "LOCKED", "ACCOUNT_LOCKED", failure.failures); logService_->security("ACCOUNT_LOCKED", "HIGH", username, "连续登录失败达到锁定阈值"); }
            Logger::warning(QStringLiteral("admin-auth"),
                            QStringLiteral("Login locked after repeated failures"));
            return ServiceResult<Admin>::fail(
                BusinessErrorCode::AdminLocked,
                QStringLiteral("登录失败次数过多，请30秒后重试"));
        }
        if (logService_) { logService_->login(found ? admin.id : 0, username, "FAILED", found ? "WRONG_PASSWORD" : "USER_NOT_FOUND", failure.failures); if (!found || failure.failures < 5) logService_->security("LOGIN_FAILED", "MEDIUM", username, "管理员登录失败"); }
        Logger::warning(QStringLiteral("admin-auth"),
                        QStringLiteral("Login rejected: invalid credentials"));
        return ServiceResult<Admin>::fail(
            BusinessErrorCode::AdminInvalidCredentials,
            QStringLiteral("账号或密码错误"));
    }
    loginStates_.remove(username);
    if (logService_) logService_->login(admin.id, username, "SUCCESS", QString(), 0);
    Logger::info(QStringLiteral("admin-auth"), QStringLiteral("Login succeeded"));
    return ServiceResult<Admin>::ok(admin);
}

ServiceResult<AdminSummary> AdminService::summary(qint64 adminId) const
{
    if (adminId <= 0) {
        return ServiceResult<AdminSummary>::fail(
            BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    }
    AdminSummary value;
    QString error;
    if (!repository_.chargerCounts(&value.onlineChargers, &value.totalChargers,
                                   &error)) {
        return ServiceResult<AdminSummary>::fail(
            BusinessErrorCode::DatabaseError, error);
    }
    value.databasePath = database_.databasePath();
    return ServiceResult<AdminSummary>::ok(value);
}

ServiceResult<RevenueSummary> AdminService::revenueSummary(qint64 adminId) const
{
    if (adminId <= 0 || !revenueService_) {
        return ServiceResult<RevenueSummary>::fail(
            BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    }
    return revenueService_->summary();
}

ServiceResult<RevenueTrend> AdminService::revenueTrend(qint64 adminId, int days) const
{
    if (adminId <= 0 || !revenueService_) {
        return ServiceResult<RevenueTrend>::fail(
            BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    }
    return revenueService_->trend(days);
}

ServiceResult<RecentOrders> AdminService::recentOrders(qint64 adminId) const
{
    if (adminId <= 0 || !revenueService_) {
        return ServiceResult<RecentOrders>::fail(
            BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    }
    return revenueService_->recentOrders();
}

ServiceResult<ChargerStatusSummary> AdminService::chargerStatusSummary(qint64 adminId) const
{
    if (adminId <= 0 || !chargerService_) {
        return ServiceResult<ChargerStatusSummary>::fail(
            BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    }
    return chargerService_->statusSummary();
}

ServiceResult<QVector<Charger>> AdminService::chargerList(qint64 adminId,
                                                          const QString &keyword,
                                                          int status, qint64 stationId) const
{
    if (adminId <= 0 || !chargerService_) {
        return ServiceResult<QVector<Charger>>::fail(
            BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    }
    return chargerService_->list(keyword, status, stationId);
}

ServiceResult<Charger> AdminService::createCharger(qint64 adminId, qint64 stationId,
                                                   const QString &code, int type,
                                                   double powerKw) const
{
    if (adminId <= 0 || !chargerService_) {
        return ServiceResult<Charger>::fail(
            BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    }
    const auto result = chargerService_->create(stationId, code, type, powerKw);
    if (logService_) logService_->operation(adminId, "CHARGER", "CREATE", "CHARGER", QString::number(stationId), "{}", result.success, result.message);
    return result;
}

ServiceResult<Charger> AdminService::deleteCharger(qint64 adminId, qint64 chargerId) const
{
    if (adminId <= 0 || !chargerService_) {
        return ServiceResult<Charger>::fail(
            BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    }
    const auto result = chargerService_->remove(chargerId);
    if (logService_) logService_->operation(adminId, "CHARGER", "DELETE", "CHARGER", QString::number(chargerId), "{}", result.success, result.message);
    return result;
}

ServiceResult<Charger> AdminService::markChargerFault(qint64 adminId, qint64 chargerId) const
{
    if (adminId <= 0 || !chargerService_) {
        return ServiceResult<Charger>::fail(
            BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    }
    const auto result = chargerService_->markFault(chargerId);
    if (logService_) logService_->operation(adminId, "CHARGER", "SET_FAULT", "CHARGER", QString::number(chargerId), "{}", result.success, result.message);
    return result;
}

ServiceResult<Charger> AdminService::recoverCharger(qint64 adminId, qint64 chargerId) const
{
    if (adminId <= 0 || !chargerService_) {
        return ServiceResult<Charger>::fail(
            BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    }
    const auto result = chargerService_->recover(chargerId);
    if (logService_) logService_->operation(adminId, "CHARGER", "RECOVER", "CHARGER", QString::number(chargerId), "{}", result.success, result.message);
    return result;
}

ServiceResult<Charger> AdminService::restartCharger(qint64 adminId, qint64 chargerId) const
{
    if (adminId <= 0 || !chargerService_) {
        return ServiceResult<Charger>::fail(
            BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    }
    const auto result = chargerService_->restart(chargerId);
    if (logService_) logService_->operation(adminId, "CHARGER", "RESTART", "CHARGER", QString::number(chargerId), "{}", result.success, result.message);
    return result;
}

int AdminService::retryAfterSeconds(const QString &username) const
{
    const auto state = loginStates_.constFind(username.trimmed());
    if (state == loginStates_.constEnd()) return 0;
    return qMax(0, QDateTime::currentDateTimeUtc().secsTo(state->lockedUntil));
}

ServiceResult<QVector<Station>> AdminService::stationList(qint64 adminId,
                                                          const QString &keyword) const
{
    if (adminId <= 0 || !stationRepository_)
        return ServiceResult<QVector<Station>>::fail(BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    QVector<Station> stations; QString error;
    if (!stationRepository_->list(&stations, &error))
        return ServiceResult<QVector<Station>>::fail(BusinessErrorCode::DatabaseError, error);
    const QString needle = keyword.trimmed();
    if (!needle.isEmpty()) {
        QVector<Station> filtered;
        for (const Station &station : stations)
            if (station.name.contains(needle, Qt::CaseInsensitive)
                || station.address.contains(needle, Qt::CaseInsensitive)) filtered.append(station);
        stations = filtered;
    }
    return ServiceResult<QVector<Station>>::ok(stations);
}

static bool validStation(const Station &station)
{
    return station.name.trimmed().size() > 0 && station.name.trimmed().size() <= 128
        && station.address.trimmed().size() > 0 && station.address.trimmed().size() <= 255
        && std::isfinite(station.longitude) && station.longitude >= -180 && station.longitude <= 180
        && std::isfinite(station.latitude) && station.latitude >= -90 && station.latitude <= 90
        && std::isfinite(station.price) && station.price > 0 && station.price <= 100000
        && station.totalSlots >= 0;
}

ServiceResult<Station> AdminService::createStation(qint64 adminId, Station station) const
{
    if (adminId <= 0 || !stationRepository_)
        return ServiceResult<Station>::fail(BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    station.name = station.name.trimmed(); station.address = station.address.trimmed();
    if (!validStation(station)) return ServiceResult<Station>::fail(BusinessErrorCode::InvalidArgument, QStringLiteral("站点参数无效"));
    QString error; Station created;
    if (!stationRepository_->insert(station, &created, &error))
        return ServiceResult<Station>::fail(BusinessErrorCode::DatabaseError, error);
    if (logService_) logService_->operation(adminId, "STATION", "CREATE", "STATION", QString::number(created.id), "{}", true);
    return ServiceResult<Station>::ok(created);
}

ServiceResult<Station> AdminService::updateStation(qint64 adminId, Station station) const
{
    if (adminId <= 0 || !stationRepository_)
        return ServiceResult<Station>::fail(BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    station.name = station.name.trimmed(); station.address = station.address.trimmed();
    if (station.id <= 0 || !validStation(station))
        return ServiceResult<Station>::fail(BusinessErrorCode::InvalidArgument, QStringLiteral("站点参数无效"));
    QString error; Station updated; bool found = false;
    if (!stationRepository_->update(station, &updated, &found, &error))
        return ServiceResult<Station>::fail(BusinessErrorCode::DatabaseError, error);
    if (!found) return ServiceResult<Station>::fail(BusinessErrorCode::StationNotFound, QStringLiteral("充电站不存在"));
    if (logService_) logService_->operation(adminId, "STATION", "UPDATE", "STATION", QString::number(updated.id), "{}", true);
    return ServiceResult<Station>::ok(updated);
}

ServiceResult<Station> AdminService::deleteStation(qint64 adminId, qint64 stationId) const
{
    if (adminId <= 0 || !stationRepository_)
        return ServiceResult<Station>::fail(BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    Station station; bool found = false; QString error;
    if (!stationRepository_->findById(stationId, &station, &found, &error))
        return ServiceResult<Station>::fail(BusinessErrorCode::DatabaseError, error);
    if (!found) return ServiceResult<Station>::fail(BusinessErrorCode::StationNotFound, QStringLiteral("充电站不存在"));
    int count = 0;
    if (!stationRepository_->chargerCount(stationId, &count, &error))
        return ServiceResult<Station>::fail(BusinessErrorCode::DatabaseError, error);
    if (count > 0) return ServiceResult<Station>::fail(BusinessErrorCode::StationHasChargers, QStringLiteral("站点仍有电桩，不能删除"));
    bool removed = false;
    if (!stationRepository_->remove(stationId, &removed, &error))
        return ServiceResult<Station>::fail(BusinessErrorCode::DatabaseError, error);
    if (removed && logService_) logService_->operation(adminId, "STATION", "DELETE", "STATION", QString::number(stationId), "{}", true);
    return removed ? ServiceResult<Station>::ok(station)
                   : ServiceResult<Station>::fail(BusinessErrorCode::StationNotFound, QStringLiteral("充电站不存在"));
}

ServiceResult<QVector<Charger>> AdminService::batchCreateChargers(qint64 adminId, qint64 stationId,
                                                                  const QString &prefix, int count,
                                                                  int type, double powerKw) const
{
    if (adminId <= 0 || !chargerService_)
        return ServiceResult<QVector<Charger>>::fail(BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    const auto result = chargerService_->batchCreate(stationId, prefix, count, type, powerKw);
    if (logService_) logService_->operation(adminId, "STATION", "BATCH_CREATE", "STATION", QString::number(stationId), "{}", result.success, result.message);
    return result;
}

ServiceResult<QVector<User>> AdminService::userList(qint64 adminId, const QString &keyword) const
{
    if (adminId <= 0 || !userRepository_)
        return ServiceResult<QVector<User>>::fail(BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    QVector<User> users; QString error;
    if (!userRepository_->list(keyword.trimmed(), &users, &error))
        return ServiceResult<QVector<User>>::fail(BusinessErrorCode::DatabaseError, error);
    return ServiceResult<QVector<User>>::ok(users);
}

ServiceResult<User> AdminService::freezeUser(qint64 adminId, qint64 userId) const
{
    if (adminId <= 0 || !userRepository_)
        return ServiceResult<User>::fail(BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    User user; bool found = false; QString error;
    if (!userRepository_->findById(userId, &user, &found, &error))
        return ServiceResult<User>::fail(BusinessErrorCode::DatabaseError, error);
    if (!found) return ServiceResult<User>::fail(BusinessErrorCode::UserNotFound, QStringLiteral("用户不存在"));
    if (user.status != 1) return ServiceResult<User>::fail(BusinessErrorCode::InvalidArgument, QStringLiteral("用户当前已冻结"));
    bool updated = false;
    if (!userRepository_->updateStatus(userId, 1, 0, &updated, &error))
        return ServiceResult<User>::fail(BusinessErrorCode::DatabaseError, error);
    if (!updated) return ServiceResult<User>::fail(BusinessErrorCode::InvalidArgument, QStringLiteral("用户状态已发生变化"));
    user.status = 0;
    if (logService_) logService_->operation(adminId, "USER", "FREEZE", "USER", QString::number(userId), "{}", true);
    return ServiceResult<User>::ok(user);
}

ServiceResult<User> AdminService::unfreezeUser(qint64 adminId, qint64 userId) const
{
    if (adminId <= 0 || !userRepository_)
        return ServiceResult<User>::fail(BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    User user; bool found = false; QString error;
    if (!userRepository_->findById(userId, &user, &found, &error))
        return ServiceResult<User>::fail(BusinessErrorCode::DatabaseError, error);
    if (!found) return ServiceResult<User>::fail(BusinessErrorCode::UserNotFound, QStringLiteral("用户不存在"));
    if (user.status != 0) return ServiceResult<User>::fail(BusinessErrorCode::InvalidArgument, QStringLiteral("用户当前未冻结"));
    bool updated = false;
    if (!userRepository_->updateStatus(userId, 0, 1, &updated, &error))
        return ServiceResult<User>::fail(BusinessErrorCode::DatabaseError, error);
    if (!updated) return ServiceResult<User>::fail(BusinessErrorCode::InvalidArgument, QStringLiteral("用户状态已发生变化"));
    user.status = 1;
    if (logService_) logService_->operation(adminId, "USER", "UNFREEZE", "USER", QString::number(userId), "{}", true);
    return ServiceResult<User>::ok(user);
}

ServiceResult<QVector<ChargingRecord>> AdminService::userOrders(qint64 adminId, qint64 userId) const
{
    if (adminId <= 0 || !userRepository_ || !chargeRepository_)
        return ServiceResult<QVector<ChargingRecord>>::fail(BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    User user; bool found = false; QString error;
    if (!userRepository_->findById(userId, &user, &found, &error))
        return ServiceResult<QVector<ChargingRecord>>::fail(BusinessErrorCode::DatabaseError, error);
    if (!found) return ServiceResult<QVector<ChargingRecord>>::fail(BusinessErrorCode::UserNotFound, QStringLiteral("用户不存在"));
    QVector<ChargingRecord> records;
    if (!chargeRepository_->listForUser(userId, 100, 0, &records, &error))
        return ServiceResult<QVector<ChargingRecord>>::fail(BusinessErrorCode::DatabaseError, error);
    return ServiceResult<QVector<ChargingRecord>>::ok(records);
}

ServiceResult<PredictionBundle> AdminService::predictionList(qint64 adminId) const
{
    if (adminId <= 0 || !predictionService_) {
        return ServiceResult<PredictionBundle>::fail(
            BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    }
    return predictionService_->list();
}

ServiceResult<bool> AdminService::runPrediction(qint64 adminId)
{
    if (adminId <= 0 || !predictionService_) {
        return ServiceResult<bool>::fail(
            BusinessErrorCode::Forbidden, QStringLiteral("管理员身份无效"));
    }
    return predictionService_->runScript();
}

}
