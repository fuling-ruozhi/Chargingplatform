#include "navigation_launcher.h"

#include <QDesktopServices>

#ifdef NCS_HAS_DBUS
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QVariantMap>
#endif

namespace ncs {

namespace {

bool openViaPortal(const QUrl &url, quintptr windowId)
{
#ifdef NCS_HAS_DBUS
    if (!QDBusConnection::sessionBus().isConnected()) {
        return false;
    }
    QDBusInterface portal(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.OpenURI"),
        QDBusConnection::sessionBus());
    QVariantMap options;
    options.insert(QStringLiteral("handle_token"),
                   QStringLiteral("ncs_nav_%1").arg(windowId));
    QDBusReply<QDBusObjectPath> reply = portal.call(
        QStringLiteral("OpenURI"),
        QStringLiteral("x11:0x%1").arg(windowId, 0, 16),
        url.toString(), options);
    // isValid() 仅保证是合法回复（可能是错误消息）；需再确认取回了对象路径。
    return reply.isValid() && !reply.value().path().isEmpty();
#else
    Q_UNUSED(url);
    Q_UNUSED(windowId);
    return false;
#endif
}

}  // namespace

bool openExternalUrl(const QUrl &url, quintptr windowId)
{
    if (openViaPortal(url, windowId)) {
        return true;
    }
    return QDesktopServices::openUrl(url);
}

}  // namespace ncs
