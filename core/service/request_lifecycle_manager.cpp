#include "request_lifecycle_manager.h"

#include "util/logger.h"

#include <QDateTime>
#include <QTimer>

#include <limits>

namespace ncs {

RequestLifecycleManager::RequestLifecycleManager(const QString &logModule,
                                                 QObject *parent)
    : QObject(parent), logModule_(logModule), timeoutTimer_(new QTimer(this))
{
    qRegisterMetaType<ncs::AsyncRequestContext>();
    timeoutTimer_->setSingleShot(true);
    connect(timeoutTimer_, &QTimer::timeout,
            this, &RequestLifecycleManager::expireDue);
}

bool RequestLifecycleManager::track(const QString &requestId,
                                    const QString &route,
                                    DuplicatePolicy policy, int timeoutMs)
{
    if (requestId.isEmpty()) return false;
    if (!prepare(route, policy)) return false;

    AsyncRequestContext context;
    context.requestId = requestId;
    context.route = route;
    context.sessionGeneration = sessionGeneration_;
    context.operationGeneration = ++operationGenerations_[route];
    context.createdAtMs = QDateTime::currentMSecsSinceEpoch();
    context.timeoutAtMs = context.createdAtMs + qMax(1, timeoutMs);
    pending_.insert(requestId, context);
    Logger::info(logModule_,
                 QStringLiteral("REQ SEND route=%1 request_id=%2 generation=%3")
                     .arg(route, requestId)
                     .arg(sessionGeneration_));
    scheduleNextTimeout();
    return true;
}

bool RequestLifecycleManager::prepare(const QString &route,
                                      DuplicatePolicy policy)
{
    if (policy == DuplicatePolicy::Reject && hasPendingRoute(route)) {
        Logger::warning(logModule_,
                        QStringLiteral("REQ CANCEL route=%1 reason=duplicate")
                            .arg(route));
        emit duplicateSuppressed(route);
        return false;
    }
    if (policy == DuplicatePolicy::Supersede) {
        bool removed = false;
        const auto ids = pending_.keys();
        for (const QString &id : ids) {
            if (pending_.value(id).route != route) continue;
            pending_.remove(id);
            retire(id);
            removed = true;
            Logger::info(logModule_,
                         QStringLiteral("REQ CANCEL route=%1 request_id=%2 reason=superseded")
                             .arg(route, id));
        }
        if (removed) scheduleNextTimeout();
    }

    return true;
}

std::optional<AsyncRequestContext> RequestLifecycleManager::complete(
    const QString &requestId)
{
    const auto iterator = pending_.find(requestId);
    if (iterator == pending_.end()) return std::nullopt;
    const AsyncRequestContext context = iterator.value();
    pending_.erase(iterator);
    scheduleNextTimeout();
    if (context.sessionGeneration != sessionGeneration_) {
        retire(requestId);
        Logger::warning(logModule_,
                        QStringLiteral("REQ STALE route=%1 request_id=%2")
                            .arg(context.route, requestId));
        return std::nullopt;
    }
    Logger::info(logModule_,
                 QStringLiteral("REQ COMPLETE route=%1 request_id=%2")
                     .arg(context.route, requestId));
    return context;
}

QVector<AsyncRequestContext> RequestLifecycleManager::advanceSession(
    const QString &reason)
{
    Logger::info(logModule_,
                 QStringLiteral("SESSION END generation=%1 reason=%2")
                     .arg(sessionGeneration_).arg(reason));
    QVector<AsyncRequestContext> cancelled = cancelPending(reason);
    ++sessionGeneration_;
    return cancelled;
}

void RequestLifecycleManager::sessionStarted()
{
    Logger::info(logModule_,
                 QStringLiteral("SESSION START generation=%1")
                     .arg(sessionGeneration_));
}

bool RequestLifecycleManager::consumeRetired(const QString &requestId)
{
    if (!retired_.remove(requestId)) return false;
    Logger::warning(logModule_,
                    QStringLiteral("REQ STALE request_id=%1").arg(requestId));
    return true;
}

bool RequestLifecycleManager::hasPendingRoute(const QString &route) const
{
    for (const AsyncRequestContext &context : pending_) {
        if (context.route == route) return true;
    }
    return false;
}

int RequestLifecycleManager::pendingCount() const
{
    return pending_.size();
}

quint64 RequestLifecycleManager::sessionGeneration() const
{
    return sessionGeneration_;
}

void RequestLifecycleManager::expireNow(const QString &requestId)
{
    const auto iterator = pending_.find(requestId);
    if (iterator == pending_.end()) return;
    const AsyncRequestContext context = iterator.value();
    pending_.erase(iterator);
    retire(requestId);
    scheduleNextTimeout();
    Logger::warning(logModule_,
                    QStringLiteral("REQ TIMEOUT route=%1 request_id=%2")
                        .arg(context.route, requestId));
    emit requestTimedOut(context);
}

void RequestLifecycleManager::expireDue()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const auto ids = pending_.keys();
    for (const QString &id : ids) {
        if (pending_.value(id).timeoutAtMs <= now) expireNow(id);
    }
    scheduleNextTimeout();
}

void RequestLifecycleManager::scheduleNextTimeout()
{
    if (pending_.isEmpty()) {
        timeoutTimer_->stop();
        return;
    }
    qint64 next = std::numeric_limits<qint64>::max();
    for (const AsyncRequestContext &context : pending_) {
        next = qMin(next, context.timeoutAtMs);
    }
    const qint64 remaining = qMax<qint64>(
        1, next - QDateTime::currentMSecsSinceEpoch());
    timeoutTimer_->start(static_cast<int>(
        qMin<qint64>(remaining, std::numeric_limits<int>::max())));
}

void RequestLifecycleManager::retire(const QString &requestId)
{
    if (retired_.contains(requestId)) return;
    retired_.insert(requestId);
    retiredOrder_.enqueue(requestId);
    while (retiredOrder_.size() > 2048) {
        retired_.remove(retiredOrder_.dequeue());
    }
}

QVector<AsyncRequestContext> RequestLifecycleManager::cancelPending(
    const QString &reason)
{
    QVector<AsyncRequestContext> cancelled;
    cancelled.reserve(pending_.size());
    for (const AsyncRequestContext &context : pending_) {
        cancelled.append(context);
        retire(context.requestId);
        Logger::info(logModule_,
                     QStringLiteral("REQ CANCEL route=%1 request_id=%2 reason=%3")
                         .arg(context.route, context.requestId, reason));
    }
    pending_.clear();
    timeoutTimer_->stop();
    return cancelled;
}

}
