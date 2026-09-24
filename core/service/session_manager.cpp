#include "session_manager.h"

#include <QRandomGenerator>

namespace ncs {

QString SessionManager::secureToken()
{
    QString token;
    token.reserve(64);
    for (int part = 0; part < 4; ++part) {
        token.append(QString::number(QRandomGenerator::system()->generate64(), 16)
                         .rightJustified(16, QLatin1Char('0')));
    }
    return token;
}

QString SessionManager::issueUserToken(qint64 userId)
{
    QWriteLocker locker(&lock_);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (auto it = userSessions_.begin(); it != userSessions_.end();) {
        if (it->expiresAt <= now) it = userSessions_.erase(it);
        else ++it;
    }
    QString token;
    do token = secureToken(); while (userSessions_.contains(token));
    userSessions_.insert(token, {userId, now.addDays(1)});
    return token;
}

bool SessionManager::resolveUser(const QString &token, qint64 *userId,
                                 BusinessErrorCode *code, QString *message)
{
    QWriteLocker locker(&lock_);
    if (token.isEmpty()) {
        *code = BusinessErrorCode::AuthRequired;
        *message = QStringLiteral("请先登录");
        return false;
    }
    auto it = userSessions_.find(token);
    if (it == userSessions_.end()) {
        *code = BusinessErrorCode::SessionExpired;
        *message = QStringLiteral("登录状态已失效，请重新登录");
        return false;
    }
    if (it->expiresAt <= QDateTime::currentDateTimeUtc()) {
        userSessions_.erase(it);
        *code = BusinessErrorCode::SessionExpired;
        *message = QStringLiteral("登录状态已过期，请重新登录");
        return false;
    }
    *userId = it->principalId;
    *code = BusinessErrorCode::Ok;
    message->clear();
    return true;
}

bool SessionManager::invalidateUser(const QString &token)
{
    QWriteLocker locker(&lock_);
    return userSessions_.remove(token) > 0;
}

QString SessionManager::issueAdminToken(qint64 adminId)
{
    QWriteLocker locker(&lock_);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (auto it = adminSessions_.begin(); it != adminSessions_.end();) {
        if (it->expiresAt <= now) it = adminSessions_.erase(it);
        else ++it;
    }
    QString token;
    do token = secureToken(); while (adminSessions_.contains(token));
    adminSessions_.insert(token, {adminId, now.addDays(1)});
    return token;
}

bool SessionManager::resolveAdmin(const QString &token, qint64 *adminId,
                                  BusinessErrorCode *code, QString *message)
{
    QWriteLocker locker(&lock_);
    if (token.isEmpty()) {
        *code = BusinessErrorCode::AuthRequired;
        *message = QStringLiteral("请先登录管理端");
        return false;
    }
    auto it = adminSessions_.find(token);
    if (it == adminSessions_.end()) {
        *code = BusinessErrorCode::SessionExpired;
        *message = QStringLiteral("管理员登录状态已失效，请重新登录");
        return false;
    }
    if (it->expiresAt <= QDateTime::currentDateTimeUtc()) {
        adminSessions_.erase(it);
        *code = BusinessErrorCode::SessionExpired;
        *message = QStringLiteral("管理员登录状态已过期，请重新登录");
        return false;
    }
    *adminId = it->principalId;
    *code = BusinessErrorCode::Ok;
    message->clear();
    return true;
}

bool SessionManager::invalidateAdmin(const QString &token)
{
    QWriteLocker locker(&lock_);
    return adminSessions_.remove(token) > 0;
}

}
