#include "user_repository.h"

#include "database/database_manager.h"
#include "model/vehicle_profile.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVector>

namespace ncs {
namespace {

const QString formalUserColumns = QStringLiteral(
    "id,COALESCE(username,''),COALESCE(phone,''),nickname,avatar_path,"
    "balance,status,created_at");

bool prepare(QSqlQuery *query, const QString &sql, QString *error)
{
    if (query->prepare(sql)) return true;
    *error = query->lastError().text();
    return false;
}

bool execute(QSqlQuery *query, QString *error)
{
    if (query->exec()) return true;
    *error = query->lastError().text();
    return false;
}

QString localNow()
{
    return QDateTime::currentDateTime().toString(
        QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
}

}

UserRepository::UserRepository(DatabaseManager &database) : db_(database) {}

void UserRepository::readUser(QSqlQuery &query, User *user)
{
    user->id = query.value(0).toLongLong();
    user->username = query.value(1).toString();
    user->phone = query.value(2).toString();
    user->nickname = query.value(3).toString();
    user->avatarPath = query.value(4).toString();
    user->balance = query.value(5).toDouble();
    user->status = query.value(6).toInt();
    user->createdAt = query.value(7).toString();
}

bool UserRepository::findByPhone(const QString &phone, User *user, bool *found,
                                 QString *error) const
{
    QSqlQuery query(db_.connection());
    if (!prepare(&query, QStringLiteral("SELECT %1 FROM user WHERE phone=:phone")
                             .arg(formalUserColumns), error)) return false;
    query.bindValue(QStringLiteral(":phone"), phone);
    if (!execute(&query, error)) return false;
    *found = query.next();
    if (*found) readUser(query, user);
    return true;
}

bool UserRepository::findById(qint64 userId, User *user, bool *found,
                              QString *error) const
{
    QSqlQuery query(db_.connection());
    if (!prepare(&query, QStringLiteral("SELECT %1 FROM user WHERE id=:id")
                             .arg(formalUserColumns), error)) return false;
    query.bindValue(QStringLiteral(":id"), userId);
    if (!execute(&query, error)) return false;
    *found = query.next();
    if (*found) readUser(query, user);
    return true;
}

bool UserRepository::list(const QString &keyword, QVector<User> *users, QString *error) const
{
    QSqlQuery query(db_.connection());
    if (!prepare(&query, QStringLiteral(
            "SELECT %1 FROM user WHERE (:keyword='' OR CAST(id AS TEXT) LIKE :pattern "
            "OR COALESCE(username,'') LIKE :pattern OR COALESCE(phone,'') LIKE :pattern) "
            "ORDER BY id").arg(formalUserColumns), error)) return false;
    query.bindValue(QStringLiteral(":keyword"), keyword);
    query.bindValue(QStringLiteral(":pattern"), QStringLiteral("%") + keyword + "%");
    if (!execute(&query, error)) return false;
    users->clear();
    while (query.next()) { User user; readUser(query, &user); users->append(user); }
    return true;
}

bool UserRepository::updateStatus(qint64 userId, int expected, int next, bool *updated,
                                  QString *error) const
{
    QSqlQuery query(db_.connection());
    if (!prepare(&query, QStringLiteral(
            "UPDATE user SET status=:next WHERE id=:id AND status=:expected"), error)) return false;
    query.bindValue(QStringLiteral(":id"), userId);
    query.bindValue(QStringLiteral(":expected"), expected);
    query.bindValue(QStringLiteral(":next"), next);
    if (!execute(&query, error)) return false;
    *updated = query.numRowsAffected() == 1;
    return true;
}

bool UserRepository::createByPhone(const QString &phone, const QString &nickname,
                                   User *user, QString *error) const
{
    QSqlQuery query(db_.connection());
    if (!prepare(&query, QStringLiteral(
            "INSERT INTO user(username,phone,nickname,avatar_path,balance,status,created_at) "
            "VALUES(:legacy_username,:phone,:nickname,'',0,1,:created_at)"), error)) return false;
    const QString createdAt = localNow();
    query.bindValue(QStringLiteral(":legacy_username"), phone);
    query.bindValue(QStringLiteral(":phone"), phone);
    query.bindValue(QStringLiteral(":nickname"), nickname);
    query.bindValue(QStringLiteral(":created_at"), createdAt);
    if (!execute(&query, error)) return false;
    user->id = query.lastInsertId().toLongLong();
    user->phone = phone;
    user->nickname = nickname;
    user->status = 1;
    user->createdAt = createdAt;
    return true;
}

bool UserRepository::updateNickname(qint64 userId, const QString &nickname,
                                    QString *error) const
{
    QSqlQuery query(db_.connection());
    if (!prepare(&query, QStringLiteral(
            "UPDATE user SET nickname=:nickname WHERE id=:id"), error)) return false;
    query.bindValue(QStringLiteral(":nickname"), nickname);
    query.bindValue(QStringLiteral(":id"), userId);
    return execute(&query, error);
}

bool UserRepository::updateAvatar(qint64 userId, const QString &avatarPath,
                                  QString *error) const
{
    QSqlQuery query(db_.connection());
    if (!prepare(&query, QStringLiteral(
            "UPDATE user SET avatar_path=:avatar_path WHERE id=:id"), error)) return false;
    query.bindValue(QStringLiteral(":avatar_path"), avatarPath);
    query.bindValue(QStringLiteral(":id"), userId);
    return execute(&query, error);
}

bool UserRepository::updateBalance(qint64 userId, double balance,
                                   QString *error) const
{
    QSqlQuery query(db_.connection());
    if (!prepare(&query, QStringLiteral(
            "UPDATE user SET balance=:balance WHERE id=:id"), error)) return false;
    query.bindValue(QStringLiteral(":balance"), balance);
    query.bindValue(QStringLiteral(":id"), userId);
    return execute(&query, error);
}

bool UserRepository::createRechargeLog(qint64 userId, double amount, double before,
                                       double after, qint64 *logId,
                                       QString *error) const
{
    QSqlQuery query(db_.connection());
    if (!prepare(&query, QStringLiteral(
            "INSERT INTO recharge_log(user_id,amount,balance_before,balance_after,created_at) "
            "VALUES(:user_id,:amount,:before,:after,:created_at)"), error)) return false;
    query.bindValue(QStringLiteral(":user_id"), userId);
    query.bindValue(QStringLiteral(":amount"), amount);
    query.bindValue(QStringLiteral(":before"), before);
    query.bindValue(QStringLiteral(":after"), after);
    query.bindValue(QStringLiteral(":created_at"), localNow());
    if (!execute(&query, error)) return false;
    *logId = query.lastInsertId().toLongLong();
    return true;
}

bool UserRepository::findVehicleProfile(qint64 userId, VehicleProfile *profile,
                                        bool *found, QString *error) const
{
    QSqlQuery query(db_.connection());
    if (!prepare(&query, QStringLiteral(
            "SELECT user_id,battery_capacity_kwh,target_soc,min_balance_reserve,"
            "usual_leave_time,charge_mode,updated_at FROM vehicle_profile "
            "WHERE user_id=:user_id"), error)) return false;
    query.bindValue(QStringLiteral(":user_id"), userId);
    if (!execute(&query, error)) return false;
    *found = query.next();
    if (*found) {
        profile->userId = query.value(0).toLongLong();
        profile->batteryCapacityKwh = query.value(1).toDouble();
        profile->targetSoc = query.value(2).toDouble();
        profile->minBalanceReserve = query.value(3).toDouble();
        profile->usualLeaveTime = query.value(4).toString();
        profile->chargeMode = query.value(5).toInt();
        profile->updatedAt = query.value(6).toString();
    }
    return true;
}

bool UserRepository::upsertVehicleProfile(const VehicleProfile &profile,
                                          QString *error) const
{
    QSqlQuery query(db_.connection());
    if (!prepare(&query, QStringLiteral(
            "INSERT INTO vehicle_profile(user_id,battery_capacity_kwh,target_soc,"
            "min_balance_reserve,usual_leave_time,charge_mode,updated_at) "
            "VALUES(:user_id,:capacity,:target_soc,:reserve,:leave_time,:mode,:updated_at) "
            "ON CONFLICT(user_id) DO UPDATE SET "
            "battery_capacity_kwh=excluded.battery_capacity_kwh,"
            "target_soc=excluded.target_soc,"
            "min_balance_reserve=excluded.min_balance_reserve,"
            "usual_leave_time=excluded.usual_leave_time,"
            "charge_mode=excluded.charge_mode,"
            "updated_at=excluded.updated_at"), error)) return false;
    query.bindValue(QStringLiteral(":user_id"), profile.userId);
    query.bindValue(QStringLiteral(":capacity"), profile.batteryCapacityKwh);
    query.bindValue(QStringLiteral(":target_soc"), profile.targetSoc);
    query.bindValue(QStringLiteral(":reserve"), profile.minBalanceReserve);
    query.bindValue(QStringLiteral(":leave_time"), profile.usualLeaveTime);
    query.bindValue(QStringLiteral(":mode"), profile.chargeMode);
    query.bindValue(QStringLiteral(":updated_at"),
                    QDateTime::currentDateTime().toString(
                        QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")));
    return execute(&query, error);
}

bool UserRepository::find(const QString &username, User *user, QString *hash,
                          QString *salt, bool *found, QString *error) const
{
    QSqlQuery query(db_.connection());
    if (!prepare(&query, QStringLiteral(
            "SELECT %1,password_hash,salt FROM user WHERE username=:username")
            .arg(formalUserColumns), error)) return false;
    query.bindValue(QStringLiteral(":username"), username);
    if (!execute(&query, error)) return false;
    *found = query.next();
    if (*found) {
        readUser(query, user);
        *hash = query.value(8).toString();
        *salt = query.value(9).toString();
    }
    return true;
}

bool UserRepository::create(const QString &username, const QString &hash,
                            const QString &salt, const QString &nickname,
                            User *user, QString *error) const
{
    QSqlQuery query(db_.connection());
    if (!prepare(&query, QStringLiteral(
            "INSERT INTO user(username,password_hash,salt,nickname,created_at) "
            "VALUES(:username,:hash,:salt,:nickname,:created_at)"), error)) return false;
    const QString createdAt = localNow();
    query.bindValue(QStringLiteral(":username"), username);
    query.bindValue(QStringLiteral(":hash"), hash);
    query.bindValue(QStringLiteral(":salt"), salt);
    query.bindValue(QStringLiteral(":nickname"), nickname);
    query.bindValue(QStringLiteral(":created_at"), createdAt);
    if (!execute(&query, error)) return false;
    user->id = query.lastInsertId().toLongLong();
    user->username = username;
    user->nickname = nickname;
    user->createdAt = createdAt;
    return true;
}

}
