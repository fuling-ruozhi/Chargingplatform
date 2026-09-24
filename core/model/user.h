#pragma once

#include <QMetaType>
#include <QString>

namespace ncs {

struct User
{
    qint64 id = 0;
    QString username;
    QString phone;
    QString nickname;
    QString avatarPath;
    double balance = 0.0;
    int status = 1;
    QString createdAt;
};

struct OtpChallenge
{
    QString displayCode;
    int cooldownSeconds = 60;
    int expiresSeconds = 300;
};

struct RechargeResult
{
    qint64 logId = 0;
    double amount = 0.0;
    double balanceBefore = 0.0;
    double balanceAfter = 0.0;
};

}

Q_DECLARE_METATYPE(ncs::User)
Q_DECLARE_METATYPE(ncs::RechargeResult)
