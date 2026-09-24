#pragma once

#include <QDateTime>
#include <QSqlDatabase>
#include <QVector>
#include <QString>

namespace ncs {

struct AdminLoginLog { QString time, username, ip, result, reason; int failedAttempts = 0; };
struct AdminOperationLog { QString time, username, module, action, targetType, targetId, detail, result, errorMessage; };
struct SecurityLog { QString time, severity, eventType, username, ip, description; };

class LogService
{
public:
    explicit LogService(QSqlDatabase database) : database_(database) {}
    QString adminUsername(qint64 adminId) const;
    bool login(qint64 adminId, const QString &username, const QString &result,
               const QString &reason, int failedAttempts, const QString &ip = QStringLiteral("127.0.0.1"));
    bool operation(qint64 adminId, const QString &module, const QString &action,
                   const QString &targetType, const QString &targetId, const QString &detail,
                   bool success, const QString &errorMessage = QString(),
                   const QString &ip = QStringLiteral("127.0.0.1"));
    bool security(const QString &eventType, const QString &severity, const QString &username,
                  const QString &description, qint64 adminId = 0,
                  const QString &target = QString(), const QString &ip = QStringLiteral("127.0.0.1"));
    bool invalidParameter(qint64 adminId, const QString &route,
                          const QString &parameter, const QString &reason,
                          const QString &severity = QStringLiteral("LOW"));
    QVector<AdminLoginLog> loginLogs(const QString &keyword, const QString &result, int limit = 100) const;
    QVector<AdminOperationLog> operationLogs(const QString &keyword, const QString &module,
                                              const QString &result, int limit = 100) const;
    QVector<SecurityLog> securityLogs(const QString &eventType, const QString &severity, int limit = 100) const;

private:
    QSqlDatabase database_;
    static QString now();
};
}
