#pragma once

#include "model/load_prediction.h"
#include "model/service_result.h"

#include <QObject>
#include <QString>

class QProcess;
class QTimer;

namespace ncs {

class DatabaseManager;
class PredictionRepository;

// UC-A-08 / UC-M-03：读取预测结果，并支持通过 QProcess 调用 Python 脚本重新预测。
// 服务器为单线程请求模型，脚本必须异步运行：runScript() 立即返回，
// 运行状态与失败原因由 list() 携带，客户端通过 admin.prediction.list 轮询。
class PredictionService : public QObject
{
    Q_OBJECT
public:
    PredictionService(DatabaseManager &database, PredictionRepository &repository,
                      QObject *parent = nullptr);

    ServiceResult<PredictionBundle> list() const;

    // 异步触发预测脚本：立即返回（脚本已在运行时幂等成功）。
    // 启动失败等结果同样通过 list() 的 lastRunError 轮询获得。
    ServiceResult<bool> runScript();
    bool isRunning() const { return running_; }

private:
    QString resolveScriptPath() const;
    QString resolvePythonInterpreter() const;
    void startScriptProcess(const QString &script);
    void finishRun(bool success, const QString &error);

    DatabaseManager &database_;
    PredictionRepository &repository_;
    QProcess *process_ = nullptr;
    QTimer *runWatchdog_ = nullptr;
    bool running_ = false;
    bool runTimedOut_ = false;
    QString lastError_;
};

}
