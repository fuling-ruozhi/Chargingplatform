#pragma once

#include <QString>

namespace ncs {

inline QString userFacingError(const QString &message)
{
    const QString lower = message.toLower();
    if (lower.contains(QStringLiteral("timeout"))
        || message.contains(QStringLiteral("超时"))) {
        return QStringLiteral("请求超时，请确认服务端正常运行后重试");
    }
    if (lower.contains(QStringLiteral("not connected"))
        || lower.contains(QStringLiteral("disconnected"))
        || lower.contains(QStringLiteral("connection refused"))
        || message.contains(QStringLiteral("未连接"))) {
        return QStringLiteral("无法连接服务，请确认管理端已启动");
    }
    if (lower.contains(QStringLiteral("database"))
        || lower.contains(QStringLiteral("sql"))
        || lower.contains(QStringLiteral("unable to execute"))
        || lower.contains(QStringLiteral("no such"))) {
        return QStringLiteral("服务暂时不可用，请稍后重试");
    }
    if (message.isEmpty()) {
        return QStringLiteral("操作未完成，请稍后重试");
    }
    return message;
}

}  // namespace ncs
