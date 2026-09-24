#include "prediction_service.h"

#include "database/database_manager.h"
#include "repository/prediction_repository.h"
#include "util/logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

namespace ncs {
namespace {

constexpr int kScriptTimeoutMs = 300'000;
constexpr int kActualHours = 24;

QString currentAppDir()
{
    return QCoreApplication::applicationDirPath();
}

}

PredictionService::PredictionService(DatabaseManager &database,
                                     PredictionRepository &repository,
                                     QObject *parent)
    : QObject(parent), database_(database), repository_(repository)
{
}

ServiceResult<PredictionBundle> PredictionService::list() const
{
    PredictionBundle bundle;
    QString error;
    if (!repository_.list(&bundle.items, &error)) {
        Logger::error(QStringLiteral("prediction"),
                      QStringLiteral("List predictions failed: %1").arg(error));
        return ServiceResult<PredictionBundle>::fail(
            BusinessErrorCode::DatabaseError,
            QStringLiteral("读取预测数据失败：%1").arg(error));
    }
    if (!repository_.latestGeneratedAt(&bundle.generatedAt, &error)) {
        Logger::error(QStringLiteral("prediction"),
                      QStringLiteral("Read generated_at failed: %1").arg(error));
        return ServiceResult<PredictionBundle>::fail(
            BusinessErrorCode::DatabaseError,
            QStringLiteral("读取预测数据失败：%1").arg(error));
    }
    // 历史实际负荷仅用于曲线对比，读取失败不阻塞预测展示
    if (!repository_.hourlyActual(kActualHours, &bundle.actual, &error)) {
        Logger::warning(QStringLiteral("prediction"),
                        QStringLiteral("Read actual load failed: %1").arg(error));
        bundle.actual.clear();
    }
    bundle.runInProgress = running_;
    bundle.lastRunError = lastError_;
    return ServiceResult<PredictionBundle>::ok(bundle);
}

QString PredictionService::resolvePythonInterpreter() const
{
    const QString fromEnv = qEnvironmentVariable("NCS_PYTHON");
    if (!fromEnv.isEmpty()) {
        return fromEnv;
    }
    const QString python3 =
        QStandardPaths::findExecutable(QStringLiteral("python3"));
    if (!python3.isEmpty()) {
        return python3;
    }
    return QStringLiteral("python");
}

QString PredictionService::resolveScriptPath() const
{
    const QString fromEnv = qEnvironmentVariable("NCS_ML_MAIN");
    if (!fromEnv.isEmpty()) {
        return fromEnv;
    }
    // 依次尝试：工作目录、可执行目录及其向上两级（build/xxx → 工程根）
    const QDir cwd = QDir::current();
    const QDir appDir = QDir(currentAppDir());
    const QStringList candidates = {
        cwd.filePath(QStringLiteral("ml/main.py")),
        QDir(cwd.filePath(QStringLiteral(".."))).filePath(QStringLiteral("ml/main.py")),
        appDir.filePath(QStringLiteral("ml/main.py")),
        QDir(appDir.filePath(QStringLiteral("../.."))).filePath(QStringLiteral("ml/main.py")),
        QDir(appDir.filePath(QStringLiteral("../../.."))).filePath(QStringLiteral("ml/main.py"))};
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return QDir::cleanPath(candidate);
        }
    }
    return {};
}

ServiceResult<bool> PredictionService::runScript()
{
    // 单线程服务器模型：脚本必须异步执行，否则网络线程会被阻塞长达 5 分钟。
    if (running_) {
        Logger::info(QStringLiteral("prediction"),
                     QStringLiteral("Prediction script already running"));
        return ServiceResult<bool>::ok(true);
    }
    const QString script = resolveScriptPath();
    if (script.isEmpty()) {
        Logger::warning(QStringLiteral("prediction"),
                        QStringLiteral("ML script not found"));
        return ServiceResult<bool>::fail(
            BusinessErrorCode::InvalidArgument,
            QStringLiteral("未找到预测脚本 ml/main.py，可设置环境变量 NCS_ML_MAIN 指定路径"));
    }
    startScriptProcess(script);
    return ServiceResult<bool>::ok(true);
}

void PredictionService::startScriptProcess(const QString &script)
{
    // 防御性清理：覆盖 process_ 前显式回收旧进程，避免泄漏或悬空引用
    if (process_) {
        process_->disconnect(this);
        process_->kill();
        process_->deleteLater();
        process_ = nullptr;
    }
    // main.py 的 --db 是全局参数，必须放在子命令之前
    auto *process = new QProcess(this);
    process->setProgram(resolvePythonInterpreter());
    process->setArguments({script, QStringLiteral("--db"),
                           QDir(database_.databasePath()).absolutePath(),
                           QStringLiteral("predict")});
    process->setWorkingDirectory(QFileInfo(script).absolutePath());
    process->setProcessChannelMode(QProcess::MergedChannels);
    Logger::info(QStringLiteral("prediction"),
                 QStringLiteral("Running prediction script: %1").arg(script));

    running_ = true;
    runTimedOut_ = false;
    lastError_.clear();
    process_ = process;

    connect(process, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (!running_) return;
        const QString output = process_
            ? QString::fromLocal8Bit(process_->readAll()).simplified().left(200)
            : QString();
        if (runTimedOut_) {
            finishRun(false, QStringLiteral("预测脚本执行超时，已保留上次预测结果"));
            return;
        }
        const bool success = exitStatus == QProcess::NormalExit && exitCode == 0;
        finishRun(success, success
            ? QString()
            : (output.isEmpty()
                ? QStringLiteral("预测脚本执行失败，已保留上次预测结果")
                : QStringLiteral("预测脚本执行失败：%1").arg(output)));
    });
    connect(process, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
        if (!running_ || error != QProcess::FailedToStart) return;
        finishRun(false,
                  QStringLiteral("无法启动 Python 进程，请确认已安装 Python 3.10"));
    });

    // 看门狗：异步模型下脚本挂死不会阻塞服务器，但需回收进程并复位状态
    if (!runWatchdog_) {
        runWatchdog_ = new QTimer(this);
        runWatchdog_->setSingleShot(true);
        connect(runWatchdog_, &QTimer::timeout, this, [this] {
            if (!running_ || !process_) return;
            runTimedOut_ = true;
            Logger::error(QStringLiteral("prediction"),
                          QStringLiteral("Prediction script timed out"));
            process_->kill();
        });
    }
    runWatchdog_->start(kScriptTimeoutMs);
    process->start();
}

void PredictionService::finishRun(bool success, const QString &error)
{
    running_ = false;
    runTimedOut_ = false;
    if (runWatchdog_) runWatchdog_->stop();
    if (process_) {
        process_->deleteLater();
        process_ = nullptr;
    }
    if (success) {
        Logger::info(QStringLiteral("prediction"),
                     QStringLiteral("Prediction script finished"));
        return;
    }
    lastError_ = error;
    Logger::error(QStringLiteral("prediction"),
                  QStringLiteral("Prediction script failed: %1").arg(error));
}

}
