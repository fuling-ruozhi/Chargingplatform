// UC-A-08 回归测试：admin.prediction.run 必须异步执行，不得阻塞调用线程（PR 复审修复）；
// 以及 admin.prediction.list 的路由鉴权与协议字段映射
#include "database/database_manager.h"
#include "network/admin_routes.h"
#include "network/json_protocol.h"
#include "network/request_router.h"
#include "repository/admin_repository.h"
#include "repository/charger_repository.h"
#include "repository/prediction_repository.h"
#include "repository/revenue_repository.h"
#include "service/admin_service.h"
#include "service/charger_service.h"
#include "service/prediction_service.h"
#include "service/revenue_service.h"
#include "service/session_manager.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>

#include <functional>

namespace {

bool waitUntil(const std::function<bool()> &condition, int timeoutMs = 15000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
    }
    return condition();
}

bool writeFile(const QString &path, const QString &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return file.write(content.toUtf8()) >= 0;
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::DatabaseManager database(
        directory.filePath(QStringLiteral("prediction-async.db")));
    if (!database.initialize()) return 1;
    ncs::PredictionRepository repository(database);
    ncs::PredictionService service(database, repository);

    // 1) 成功脚本（sh 模拟 Python，运行约 1.5 秒）：runScript 立即返回且不阻塞，
    //    运行期间 list() 报告 running=true；重复触发幂等成功
    const QString okScript = directory.filePath(QStringLiteral("ok.sh"));
    if (!writeFile(okScript,
            QStringLiteral("#!/bin/sh\nsleep 1.5\nexit 0\n"))) return 2;
    qputenv("NCS_PYTHON", QByteArrayLiteral("/bin/sh"));
    qputenv("NCS_ML_MAIN", okScript.toUtf8());
    QElapsedTimer elapsed;
    elapsed.start();
    const auto started = service.runScript();
    if (!started.success) return 3;
    if (elapsed.elapsed() > 500) return 4;   // 必须立即返回，不得等待脚本
    if (!service.isRunning()) return 5;      // 进程应在后台运行
    const auto during = service.list();
    if (!during.success || !during.value.runInProgress
        || !during.value.lastRunError.isEmpty()) return 6;
    const auto again = service.runScript();  // 运行中重复触发：幂等成功
    if (!again.success) return 7;
    if (!waitUntil([&] { return !service.isRunning(); })) return 8;
    const auto after = service.list();
    if (!after.success || after.value.runInProgress
        || !after.value.lastRunError.isEmpty()) return 9;

    // 2) 失败脚本：结束后 running=false，lastError 携带脚本输出摘要
    const QString failScript = directory.filePath(QStringLiteral("fail.sh"));
    if (!writeFile(failScript,
            QStringLiteral("#!/bin/sh\necho boom\nexit 3\n"))) return 10;
    qputenv("NCS_ML_MAIN", failScript.toUtf8());
    if (!service.runScript().success) return 11;
    if (!waitUntil([&] { return !service.isRunning(); })) return 12;
    const auto failed = service.list();
    if (!failed.success || failed.value.runInProgress
        || !failed.value.lastRunError.contains(QStringLiteral("boom"))) return 13;

    // 3) 脚本路径不存在：resolveScriptPath 对非空环境变量不做存在性校验，
    //    因此走异步启动 → 进程退出失败路径，最终 lastError 非空
    qputenv("NCS_ML_MAIN", directory.filePath(QStringLiteral("missing.py")).toUtf8());
    const auto missing = service.runScript();
    if (!missing.success) return 14;
    if (!waitUntil([&] { return !service.isRunning(); })) return 15;
    const auto missingResult = service.list();
    if (!missingResult.success || missingResult.value.runInProgress
        || missingResult.value.lastRunError.isEmpty()) return 16;

    // 4) 路由层：admin.prediction.list 鉴权与协议字段映射
    {
        ncs::AdminRepository adminRepository(database);
        ncs::RevenueRepository revenueRepository(database);
        ncs::RevenueService revenueService(revenueRepository);
        ncs::ChargerRepository chargerRepository(database);
        ncs::ChargerService chargerService(chargerRepository);
        ncs::AdminService adminService(database, adminRepository, revenueService,
                                       chargerService);
        ncs::PredictionService routeService(database, repository);
        adminService.setPredictionService(routeService);
        ncs::SessionManager sessions;
        ncs::RequestRouter router;
        ncs::AdminRoutes::registerPredictionRoutes(router, adminService, sessions);

        // 预置一条预测（station 1 由 seed 数据提供），校验响应字段映射
        QSqlQuery seedRow(database.connection());
        if (!seedRow.exec(QStringLiteral(
                "INSERT INTO load_prediction(station_id, generated_at, target_time,"
                " horizon_hours, predicted_energy, predicted_free_chargers, is_peak)"
                " VALUES(1,'2026-09-08 10:00:00','2026-09-08 11:00:00',24,12.5,3,1)")))
            return 17;

        ncs::JsonRequest deniedRequest;
        deniedRequest.requestId = QStringLiteral("r1");
        deniedRequest.type = QStringLiteral("admin.prediction.list");
        const auto denied = router.route(deniedRequest);
        if (denied.success) return 18;  // 无 token 必须被拒

        const auto login = adminService.login(QStringLiteral("admin"),
                                              QStringLiteral("123456"));
        if (!login.success) return 19;
        ncs::JsonRequest listRequest;
        listRequest.requestId = QStringLiteral("r2");
        listRequest.type = QStringLiteral("admin.prediction.list");
        listRequest.data.insert(QStringLiteral("admin_session_token"),
                                sessions.issueAdminToken(login.value.id));
        const auto listed = router.route(listRequest);
        if (!listed.success) return 20;
        const auto items = listed.data.value(QStringLiteral("items")).toArray();
        if (items.size() != 1) return 21;
        const auto row = items.first().toObject();
        if (row.value(QStringLiteral("predicted_energy")).toDouble() != 12.5
            || row.value(QStringLiteral("horizon_hours")).toInt() != 24
            || !row.value(QStringLiteral("is_peak")).toBool()
            || row.value(QStringLiteral("station_name")).toString().isEmpty()
            || row.value(QStringLiteral("target_time")).toString()
                   != QStringLiteral("2026-09-08 11:00:00")
            || listed.data.value(QStringLiteral("generated_at")).toString()
                   != QStringLiteral("2026-09-08 10:00:00")
            || !listed.data.contains(QStringLiteral("running"))
            || listed.data.value(QStringLiteral("running")).toBool()
            || !listed.data.contains(QStringLiteral("last_error"))
            || !listed.data.value(QStringLiteral("last_error")).toString().isEmpty())
            return 22;
    }
    return 0;
}
