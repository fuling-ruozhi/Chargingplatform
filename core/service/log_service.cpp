#include "log_service.h"
#include "util/logger.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace ncs {
QString LogService::now() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); }
QString LogService::adminUsername(qint64 id) const
{
    QSqlQuery q(database_); q.prepare(QStringLiteral("SELECT username FROM admin WHERE id=:id"));
    q.bindValue(QStringLiteral(":id"), id); return q.exec() && q.next() ? q.value(0).toString() : QString();
}
bool LogService::login(qint64 id, const QString &username, const QString &result,
                       const QString &reason, int attempts, const QString &ip)
{
    QSqlQuery q(database_); q.prepare(QStringLiteral(
        "INSERT INTO admin_login_logs(admin_id,admin_username,login_time,login_result,failure_reason,client_ip,failed_attempts,created_at)"
        " VALUES(:id,:u,:t,:r,:reason,:ip,:attempts,:created)"));
    q.bindValue(":id", id > 0 ? QVariant(id) : QVariant()); q.bindValue(":u", username);
    q.bindValue(":t", now()); q.bindValue(":r", result); q.bindValue(":reason", reason.isNull() ? QStringLiteral("") : reason);
    q.bindValue(":ip", ip); q.bindValue(":attempts", attempts); q.bindValue(":created", now());
    if (q.exec()) return true; Logger::error("DB", "Login audit write failed: " + q.lastError().text()); return false;
}
bool LogService::operation(qint64 id, const QString &module, const QString &action,
                           const QString &type, const QString &target, const QString &detail,
                           bool success, const QString &message, const QString &ip)
{
    QSqlQuery q(database_); q.prepare(QStringLiteral(
        "INSERT INTO admin_operation_logs(admin_id,admin_username,module,action,target_type,target_id,detail,result,error_message,client_ip,created_at)"
        " VALUES(:id,:u,:m,:a,:tt,:tid,:d,:r,:e,:ip,:t)"));
    q.bindValue(":id", id); q.bindValue(":u", adminUsername(id)); q.bindValue(":m", module);
    q.bindValue(":a", action); q.bindValue(":tt", type); q.bindValue(":tid", target);
    q.bindValue(":d", detail.isNull() ? QStringLiteral("") : detail); q.bindValue(":r", success ? "SUCCESS" : "FAILED");
    q.bindValue(":e", message.isNull() ? QStringLiteral("") : message); q.bindValue(":ip", ip); q.bindValue(":t", now());
    if (q.exec()) return true; Logger::error("DB", "Operation audit write failed: " + q.lastError().text()); return false;
}
bool LogService::security(const QString &event, const QString &severity, const QString &username,
                          const QString &description, qint64 id, const QString &target, const QString &ip)
{
    QSqlQuery q(database_); q.prepare(QStringLiteral(
        "INSERT INTO security_logs(event_type,severity,admin_id,username,client_ip,target,description,created_at)"
        " VALUES(:e,:s,:id,:u,:ip,:target,:d,:t)"));
    q.bindValue(":e", event); q.bindValue(":s", severity); q.bindValue(":id", id > 0 ? QVariant(id) : QVariant());
    q.bindValue(":u", username.isNull() ? QStringLiteral("") : username); q.bindValue(":ip", ip); q.bindValue(":target", target.isNull() ? QStringLiteral("") : target); q.bindValue(":d", description.isNull() ? QStringLiteral("") : description); q.bindValue(":t", now());
    if (q.exec()) return true; Logger::error("DB", "Security audit write failed: " + q.lastError().text()); return false;
}
bool LogService::invalidParameter(qint64 adminId, const QString &route,
                                  const QString &parameter, const QString &reason,
                                  const QString &severity)
{
    const QString description = QStringLiteral("route=%1 parameter=%2 reason=%3")
        .arg(route, parameter, reason);
    return security(QStringLiteral("INVALID_PARAMETER"), severity,
                    adminUsername(adminId), description, adminId, route);
}
QVector<AdminLoginLog> LogService::loginLogs(const QString &keyword, const QString &result, int limit) const
{
    QVector<AdminLoginLog> out; QSqlQuery q(database_); q.prepare(QStringLiteral(
        "SELECT created_at,admin_username,client_ip,login_result,failure_reason,failed_attempts FROM admin_login_logs "
        "WHERE (:k='' OR admin_username LIKE :like) AND (:r='' OR login_result=:r) ORDER BY created_at DESC LIMIT :limit"));
    q.bindValue(":k", keyword); q.bindValue(":like", "%" + keyword + "%"); q.bindValue(":r", result); q.bindValue(":limit", qBound(1, limit, 100));
    if (!q.exec()) return out; while (q.next()) out.append({q.value(0).toString(),q.value(1).toString(),q.value(2).toString(),q.value(3).toString(),q.value(4).toString(),q.value(5).toInt()}); return out;
}
QVector<AdminOperationLog> LogService::operationLogs(const QString &keyword, const QString &module, const QString &result, int limit) const
{
    QVector<AdminOperationLog> out; QSqlQuery q(database_); q.prepare(QStringLiteral(
        "SELECT created_at,admin_username,module,action,target_type,target_id,detail,result,error_message FROM admin_operation_logs "
        "WHERE (:k='' OR admin_username LIKE :like) AND (:m='' OR module=:m) AND (:r='' OR result=:r) ORDER BY created_at DESC LIMIT :limit"));
    q.bindValue(":k", keyword); q.bindValue(":like", "%" + keyword + "%"); q.bindValue(":m", module); q.bindValue(":r", result); q.bindValue(":limit", qBound(1, limit, 100));
    if (!q.exec()) return out; while (q.next()) out.append({q.value(0).toString(),q.value(1).toString(),q.value(2).toString(),q.value(3).toString(),q.value(4).toString(),q.value(5).toString(),q.value(6).toString(),q.value(7).toString(),q.value(8).toString()}); return out;
}
QVector<SecurityLog> LogService::securityLogs(const QString &event, const QString &severity, int limit) const
{
    QVector<SecurityLog> out; QSqlQuery q(database_); q.prepare(QStringLiteral(
        "SELECT created_at,severity,event_type,username,client_ip,description FROM security_logs "
        "WHERE (:e='' OR event_type=:e) AND (:s='' OR severity=:s) ORDER BY created_at DESC LIMIT :limit"));
    q.bindValue(":e", event); q.bindValue(":s", severity); q.bindValue(":limit", qBound(1, limit, 100));
    if (!q.exec()) return out; while (q.next()) out.append({q.value(0).toString(),q.value(1).toString(),q.value(2).toString(),q.value(3).toString(),q.value(4).toString(),q.value(5).toString()}); return out;
}
}
