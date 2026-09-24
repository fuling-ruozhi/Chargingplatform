#pragma once

#include "model/business_error.h"

#include <QDateTime>
#include <QHash>
#include <QReadWriteLock>
#include <QString>

namespace ncs {

class SessionManager
{
public:
    QString issueUserToken(qint64 userId);
    bool resolveUser(const QString &token, qint64 *userId,
                     BusinessErrorCode *code, QString *message);
    bool invalidateUser(const QString &token);
    QString issueAdminToken(qint64 adminId);
    bool resolveAdmin(const QString &token, qint64 *adminId,
                      BusinessErrorCode *code, QString *message);
    bool invalidateAdmin(const QString &token);

private:
    struct Session {
        qint64 principalId = 0;
        QDateTime expiresAt;
    };

    static QString secureToken();
    mutable QReadWriteLock lock_;
    QHash<QString, Session> userSessions_;
    QHash<QString, Session> adminSessions_;
};

}
