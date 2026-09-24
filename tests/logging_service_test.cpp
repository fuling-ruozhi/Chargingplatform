#include "database/database_manager.h"
#include "network/admin_routes.h"
#include "network/request_router.h"
#include "repository/admin_repository.h"
#include "service/admin_service.h"
#include "service/log_service.h"
#include "service/session_manager.h"
#include "util/logger.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv); QTemporaryDir dir;
    ncs::DatabaseManager database(dir.filePath("logging.db"));
    if (!database.initialize() || database.schemaVersion() != 10) return 1;
    ncs::LogService logs(database.connection()); ncs::AdminRepository repository(database);
    ncs::AdminService service(database, repository); service.setLogService(logs);
    if (!service.login("admin", "123456").success) return 2;
    for (int i = 0; i < 5; ++i) service.login("admin", "wrong");
    QSqlQuery query(database.connection());
    if (!query.exec("SELECT COUNT(*) FROM admin_login_logs WHERE login_result='SUCCESS'") || !query.next() || query.value(0).toInt() < 1) return 3;
    if (!query.exec("SELECT COUNT(*) FROM admin_login_logs WHERE login_result='LOCKED'") || !query.next() || query.value(0).toInt() < 1) return 4;
    if (!query.exec("SELECT COUNT(*) FROM security_logs WHERE event_type='ACCOUNT_LOCKED'") || !query.next() || query.value(0).toInt() < 1) return 5;
    logs.operation(1, "SYSTEM", "OTHER", "TEST", "1", "{}", true);
    if (!query.exec("SELECT COUNT(*) FROM admin_operation_logs WHERE module='SYSTEM'") || !query.next() || query.value(0).toInt() < 1) return 6;
    ncs::RequestRouter router; ncs::SessionManager sessions;
    ncs::AdminRoutes::registerAll(router, service, sessions);
    const QString token = sessions.issueAdminToken(1);
    const auto invalidStation = router.route({"station-invalid", "admin.station.create",
        {{"admin_session_token", token}, {"address", "地址"}, {"longitude", 116.0},
         {"latitude", 39.0}, {"price", 1.0}, {"total_slots", 1}}});
    if (invalidStation.success) return 9;
    const auto invalidCharger = router.route({"charger-invalid", "admin.charger.delete",
        {{"admin_session_token", token}, {"charger_id", "abc"}}});
    if (invalidCharger.success) return 10;
    const auto invalidStatus = router.route({"status-invalid", "admin.charger.list",
        {{"admin_session_token", token}, {"status", 99}}});
    if (invalidStatus.success) return 13;
    const auto validList = router.route({"list-valid", "admin.charger.list",
        {{"admin_session_token", token}}});
    if (validList.success) return 11;
    if (!query.exec("SELECT COUNT(*) FROM security_logs WHERE event_type='INVALID_PARAMETER'") || !query.next() || query.value(0).toInt() < 3) return 12;
    const QString old = ncs::Logger::logDirectory(); ncs::Logger::setDirectoryOverrideForTests(dir.filePath("logs"));
    if (!ncs::Logger::write(ncs::LogLevel::Info, "TEST", "password=secret token=hidden")) return 7;
    QFile file(QDir(ncs::Logger::logDirectory()).filePath("app.log"));
    if (!file.exists() || !file.open(QIODevice::ReadOnly) || QString::fromUtf8(file.readAll()).contains("secret")) return 8;
    ncs::Logger::setDirectoryOverrideForTests(old); return 0;
}
