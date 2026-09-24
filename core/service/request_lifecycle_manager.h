#pragma once

#include <QHash>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QVector>

#include <optional>

class QTimer;

namespace ncs {

struct AsyncRequestContext
{
    QString requestId;
    QString route;
    quint64 sessionGeneration = 0;
    quint64 operationGeneration = 0;
    qint64 createdAtMs = 0;
    qint64 timeoutAtMs = 0;
};

class RequestLifecycleManager : public QObject
{
    Q_OBJECT
public:
    enum class DuplicatePolicy { Allow, Supersede, Reject };

    explicit RequestLifecycleManager(const QString &logModule,
                                     QObject *parent = nullptr);

    bool prepare(const QString &route, DuplicatePolicy policy);
    bool track(const QString &requestId, const QString &route,
               DuplicatePolicy policy, int timeoutMs = 10000);
    std::optional<AsyncRequestContext> complete(const QString &requestId);
    QVector<AsyncRequestContext> advanceSession(const QString &reason);
    void sessionStarted();
    bool consumeRetired(const QString &requestId);
    bool hasPendingRoute(const QString &route) const;
    int pendingCount() const;
    quint64 sessionGeneration() const;

    // Deterministic test hook; production expiration is timer-driven.
    void expireNow(const QString &requestId);

signals:
    void requestTimedOut(const ncs::AsyncRequestContext &context);
    void duplicateSuppressed(const QString &route);

private:
    void retire(const QString &requestId);
    QVector<AsyncRequestContext> cancelPending(const QString &reason);
    void expireDue();
    void scheduleNextTimeout();

    QString logModule_;
    QHash<QString, AsyncRequestContext> pending_;
    QHash<QString, quint64> operationGenerations_;
    QSet<QString> retired_;
    QQueue<QString> retiredOrder_;
    QTimer *timeoutTimer_ = nullptr;
    quint64 sessionGeneration_ = 1;
};

}

Q_DECLARE_METATYPE(ncs::AsyncRequestContext)
