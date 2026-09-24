#include "service/request_lifecycle_manager.h"

#include <QCoreApplication>
#include <QSet>
#include <QTimer>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    ncs::RequestLifecycleManager lifecycle(QStringLiteral("LifecycleTest"));
    int timeoutCount = 0;
    int duplicateCount = 0;
    QObject::connect(&lifecycle,
                     &ncs::RequestLifecycleManager::requestTimedOut,
                     [&](const ncs::AsyncRequestContext &) { ++timeoutCount; });
    QObject::connect(&lifecycle,
                     &ncs::RequestLifecycleManager::duplicateSuppressed,
                     [&](const QString &) { ++duplicateCount; });

    QSet<QString> ids;
    for (int index = 0; index < 100; ++index) {
        const QString id = QStringLiteral("request-%1").arg(index);
        if (ids.contains(id)) return 1;
        ids.insert(id);
        if (!lifecycle.track(id, QStringLiteral("query"),
                             ncs::RequestLifecycleManager::DuplicatePolicy::Allow))
            return 2;
    }
    if (lifecycle.pendingCount() != 100
        || lifecycle.findChildren<QTimer *>().size() != 1) return 3;
    for (const QString &id : ids) {
        if (!lifecycle.complete(id).has_value()) return 4;
    }
    if (lifecycle.pendingCount() != 0) return 5;

    if (!lifecycle.track(QStringLiteral("write-1"), QStringLiteral("recharge"),
                         ncs::RequestLifecycleManager::DuplicatePolicy::Reject))
        return 6;
    if (lifecycle.track(QStringLiteral("write-2"), QStringLiteral("recharge"),
                        ncs::RequestLifecycleManager::DuplicatePolicy::Reject))
        return 7;
    if (duplicateCount != 1 || lifecycle.pendingCount() != 1) return 8;
    lifecycle.expireNow(QStringLiteral("write-1"));
    if (timeoutCount != 1 || lifecycle.pendingCount() != 0) return 9;
    if (!lifecycle.track(QStringLiteral("write-retry"), QStringLiteral("recharge"),
                         ncs::RequestLifecycleManager::DuplicatePolicy::Reject))
        return 10;
    if (!lifecycle.complete(QStringLiteral("write-retry")).has_value()) return 11;

    if (!lifecycle.track(QStringLiteral("old-query"), QStringLiteral("station.list"),
                         ncs::RequestLifecycleManager::DuplicatePolicy::Supersede))
        return 12;
    if (!lifecycle.track(QStringLiteral("new-query"), QStringLiteral("station.list"),
                         ncs::RequestLifecycleManager::DuplicatePolicy::Supersede))
        return 13;
    if (!lifecycle.consumeRetired(QStringLiteral("old-query"))) return 14;
    if (lifecycle.complete(QStringLiteral("old-query")).has_value()) return 15;
    if (!lifecycle.complete(QStringLiteral("new-query")).has_value()) return 16;

    const quint64 generation = lifecycle.sessionGeneration();
    if (!lifecycle.track(QStringLiteral("session-old"), QStringLiteral("profile.get"),
                         ncs::RequestLifecycleManager::DuplicatePolicy::Allow))
        return 17;
    const auto cancelled = lifecycle.advanceSession(QStringLiteral("logout"));
    if (cancelled.size() != 1 || lifecycle.pendingCount() != 0
        || lifecycle.sessionGeneration() != generation + 1)
        return 18;
    if (!lifecycle.consumeRetired(QStringLiteral("session-old"))) return 19;
    if (lifecycle.complete(QStringLiteral("session-old")).has_value()) return 20;
    return 0;
}
