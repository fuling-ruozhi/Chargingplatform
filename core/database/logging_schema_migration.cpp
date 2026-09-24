#include "logging_schema_migration.h"
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
namespace ncs {
bool migrateLoggingSchema(QSqlDatabase &db, QString *error)
{
    const QStringList sql{
        "CREATE TABLE IF NOT EXISTS admin_login_logs(id INTEGER PRIMARY KEY AUTOINCREMENT,admin_id INTEGER,admin_username TEXT NOT NULL DEFAULT '',login_time TEXT NOT NULL,login_result TEXT NOT NULL,failure_reason TEXT NOT NULL DEFAULT '',client_ip TEXT NOT NULL DEFAULT '127.0.0.1',failed_attempts INTEGER NOT NULL DEFAULT 0,session_id TEXT NOT NULL DEFAULT '',logout_time TEXT,created_at TEXT NOT NULL)", "CREATE INDEX IF NOT EXISTS idx_admin_login_time ON admin_login_logs(created_at DESC)", "CREATE INDEX IF NOT EXISTS idx_admin_login_admin ON admin_login_logs(admin_id,created_at DESC)", "CREATE INDEX IF NOT EXISTS idx_admin_login_username ON admin_login_logs(admin_username)",
        "CREATE TABLE IF NOT EXISTS admin_operation_logs(id INTEGER PRIMARY KEY AUTOINCREMENT,admin_id INTEGER,admin_username TEXT NOT NULL DEFAULT '',module TEXT NOT NULL,action TEXT NOT NULL,target_type TEXT NOT NULL DEFAULT '',target_id TEXT NOT NULL DEFAULT '',detail TEXT NOT NULL DEFAULT '',result TEXT NOT NULL,error_message TEXT NOT NULL DEFAULT '',client_ip TEXT NOT NULL DEFAULT '127.0.0.1',created_at TEXT NOT NULL)", "CREATE INDEX IF NOT EXISTS idx_operation_created_at ON admin_operation_logs(created_at DESC)", "CREATE INDEX IF NOT EXISTS idx_operation_admin ON admin_operation_logs(admin_id,created_at DESC)", "CREATE INDEX IF NOT EXISTS idx_operation_module ON admin_operation_logs(module)",
        "CREATE TABLE IF NOT EXISTS security_logs(id INTEGER PRIMARY KEY AUTOINCREMENT,event_type TEXT NOT NULL,severity TEXT NOT NULL,admin_id INTEGER,username TEXT NOT NULL DEFAULT '',client_ip TEXT NOT NULL DEFAULT '127.0.0.1',target TEXT NOT NULL DEFAULT '',description TEXT NOT NULL DEFAULT '',created_at TEXT NOT NULL)", "CREATE INDEX IF NOT EXISTS idx_security_created_at ON security_logs(created_at DESC)", "CREATE INDEX IF NOT EXISTS idx_security_event_type ON security_logs(event_type)"};
    for (const QString &statement : sql) { QSqlQuery query(db); if (!query.exec(statement)) { *error = query.lastError().text(); return false; } }
    return true;
}
}
