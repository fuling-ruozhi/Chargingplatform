#pragma once

#include "model/user.h"
#include "model/vehicle_profile.h"
#include <QVector>

class QSqlQuery;

namespace ncs {

class DatabaseManager;

class UserRepository
{
public:
    explicit UserRepository(DatabaseManager &database);

    bool findByPhone(const QString &phone, User *user, bool *found,
                     QString *error) const;
    bool findById(qint64 userId, User *user, bool *found, QString *error) const;
    bool list(const QString &keyword, QVector<User> *users, QString *error) const;
    bool updateStatus(qint64 userId, int expected, int next, bool *updated,
                      QString *error) const;
    bool createByPhone(const QString &phone, const QString &nickname,
                       User *user, QString *error) const;
    bool updateNickname(qint64 userId, const QString &nickname,
                        QString *error) const;
    bool updateAvatar(qint64 userId, const QString &avatarPath,
                      QString *error) const;
    bool updateBalance(qint64 userId, double balance, QString *error) const;
    bool createRechargeLog(qint64 userId, double amount, double before,
                           double after, qint64 *logId, QString *error) const;
    bool findVehicleProfile(qint64 userId, VehicleProfile *profile, bool *found,
                            QString *error) const;
    bool upsertVehicleProfile(const VehicleProfile &profile, QString *error) const;

    bool find(const QString &username, User *user, QString *hash, QString *salt,
              bool *found, QString *error) const;
    bool create(const QString &username, const QString &hash, const QString &salt,
                const QString &nickname, User *user, QString *error) const;

private:
    static void readUser(QSqlQuery &query, User *user);
    DatabaseManager &db_;
};

}
