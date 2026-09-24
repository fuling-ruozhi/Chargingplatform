#pragma once

#include <QHash>
#include <QMutex>
#include <QString>
#include <QVector>

#include <functional>

namespace ncs {

class AuthRateLimiter
{
public:
    explicit AuthRateLimiter(std::function<qint64()> clock = {});

    bool allowOtpRequest(const QString &target);
    bool allowLogin(const QString &target);
    bool recordLoginFailure(const QString &target);
    void recordLoginSuccess(const QString &target);

private:
    struct Entry {
        QVector<qint64> otpRequests;
        int failedLogins = 0;
        qint64 lockedUntil = 0;
        qint64 lastTouched = 0;
    };

    qint64 now() const;
    void cleanupLocked(qint64 timestamp);
    Entry &entry(const QString &target, qint64 timestamp);

    std::function<qint64()> clock_;
    mutable QMutex mutex_;
    QHash<QString, Entry> entries_;
};

}
