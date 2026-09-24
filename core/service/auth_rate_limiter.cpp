#include "auth_rate_limiter.h"

#include <QDateTime>
#include <QMutexLocker>

#include <algorithm>

namespace ncs {
namespace {

constexpr qint64 OtpIntervalMs = 60 * 1000;
constexpr qint64 OtpWindowMs = 10 * 60 * 1000;
constexpr int OtpWindowLimit = 5;
constexpr qint64 LoginLockMs = 60 * 1000;
constexpr int LoginFailureLimit = 5;
constexpr int MaximumEntries = 4096;

}

AuthRateLimiter::AuthRateLimiter(std::function<qint64()> clock)
    : clock_(std::move(clock))
{
    if (!clock_) clock_ = [] { return QDateTime::currentMSecsSinceEpoch(); };
}

qint64 AuthRateLimiter::now() const
{
    return clock_();
}

void AuthRateLimiter::cleanupLocked(qint64 timestamp)
{
    for (auto it = entries_.begin(); it != entries_.end();) {
        while (!it->otpRequests.isEmpty()
               && timestamp - it->otpRequests.first() >= OtpWindowMs) {
            it->otpRequests.removeFirst();
        }
        if (it->lastTouched + OtpWindowMs <= timestamp
            && it->lockedUntil <= timestamp
            && it->otpRequests.isEmpty()) {
            it = entries_.erase(it);
        } else {
            ++it;
        }
    }
    while (entries_.size() > MaximumEntries) {
        auto oldest = entries_.begin();
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (it->lastTouched < oldest->lastTouched) oldest = it;
        }
        entries_.erase(oldest);
    }
}

AuthRateLimiter::Entry &AuthRateLimiter::entry(const QString &target, qint64 timestamp)
{
    Entry &value = entries_[target];
    value.lastTouched = timestamp;
    return value;
}

bool AuthRateLimiter::allowOtpRequest(const QString &target)
{
    const qint64 timestamp = now();
    QMutexLocker locker(&mutex_);
    cleanupLocked(timestamp);
    Entry &value = entry(target, timestamp);
    if (!value.otpRequests.isEmpty()
        && timestamp - value.otpRequests.constLast() < OtpIntervalMs) return false;
    if (value.otpRequests.size() >= OtpWindowLimit) return false;
    value.otpRequests.append(timestamp);
    return true;
}

bool AuthRateLimiter::allowLogin(const QString &target)
{
    const qint64 timestamp = now();
    QMutexLocker locker(&mutex_);
    cleanupLocked(timestamp);
    const Entry &value = entry(target, timestamp);
    return value.lockedUntil <= timestamp;
}

bool AuthRateLimiter::recordLoginFailure(const QString &target)
{
    const qint64 timestamp = now();
    QMutexLocker locker(&mutex_);
    cleanupLocked(timestamp);
    Entry &value = entry(target, timestamp);
    ++value.failedLogins;
    if (value.failedLogins < LoginFailureLimit) return false;
    value.lockedUntil = timestamp + LoginLockMs;
    value.failedLogins = 0;
    return true;
}

void AuthRateLimiter::recordLoginSuccess(const QString &target)
{
    QMutexLocker locker(&mutex_);
    auto it = entries_.find(target);
    if (it == entries_.end()) return;
    it->otpRequests.clear();
    it->failedLogins = 0;
    it->lockedUntil = 0;
}

}
