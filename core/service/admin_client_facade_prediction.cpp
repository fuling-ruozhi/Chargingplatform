// UC-A-08 管理端预测请求（NFR-M-01：与 admin_client_facade.cpp 拆分）
#include "admin_client_facade.h"

#include "model/business_error.h"

#include <QJsonArray>
#include <QJsonObject>
#include <cmath>

namespace ncs {

void AdminClientFacade::requestPredictions()
{
    // 元类型注册已移至 AdminClientFacade 构造函数，避免热路径重复注册
    sendAuthenticated(QStringLiteral("admin.prediction.list"));
}

void AdminClientFacade::runPrediction()
{
    // 服务端异步执行脚本，请求立即返回；结果通过轮询 admin.prediction.list 获取
    send(QStringLiteral("admin.prediction.run"),
         {{QStringLiteral("admin_session_token"), sessionToken_}});
}

void AdminClientFacade::handlePredictionResponse(const QString &route,
                                                 const JsonResponse &response)
{
    if (route == QStringLiteral("admin.prediction.run")) {
        // 语义：运行请求已被服务器接受（脚本开始执行），后续靠轮询拿结果
        emit predictionRunSucceeded();
        return;
    }
    const QJsonValue itemsValue = response.data.value(QStringLiteral("items"));
    const QJsonValue actualValue = response.data.value(QStringLiteral("actual"));
    if (!itemsValue.isArray() || !actualValue.isArray()) {
        emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument),
                           QStringLiteral("服务器响应格式错误"), 0);
        return;
    }
    PredictionBundle bundle;
    bundle.generatedAt =
        response.data.value(QStringLiteral("generated_at")).toString();
    for (const QJsonValue &item : itemsValue.toArray()) {
        if (!item.isObject()) {
            emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument),
                               QStringLiteral("服务器响应格式错误"), 0);
            return;
        }
        const QJsonObject object = item.toObject();
        const QJsonValue id = object.value(QStringLiteral("id"));
        const QJsonValue stationId = object.value(QStringLiteral("station_id"));
        const QJsonValue horizon = object.value(QStringLiteral("horizon_hours"));
        const QJsonValue energy = object.value(QStringLiteral("predicted_energy"));
        const QJsonValue free = object.value(QStringLiteral("predicted_free_chargers"));
        if (!id.isDouble() || !stationId.isDouble()
            || !horizon.isDouble() || horizon.toInt(-1) < 0
            || !energy.isDouble() || !std::isfinite(energy.toDouble())
            || energy.toDouble() < 0.0
            || !free.isDouble() || free.toInt(-1) < 0
            || !object.value(QStringLiteral("target_time")).isString()) {
            emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument),
                               QStringLiteral("服务器响应格式错误"), 0);
            return;
        }
        bundle.items.append({id.toInteger(), stationId.toInteger(),
                             object.value(QStringLiteral("station_name")).toString(),
                             object.value(QStringLiteral("generated_at")).toString(),
                             object.value(QStringLiteral("target_time")).toString(),
                             horizon.toInt(), energy.toDouble(), free.toInt(),
                             object.value(QStringLiteral("is_peak")).toBool()});
    }
    for (const QJsonValue &item : actualValue.toArray()) {
        if (!item.isObject()) {
            emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument),
                               QStringLiteral("服务器响应格式错误"), 0);
            return;
        }
        const QJsonObject object = item.toObject();
        const QJsonValue energy = object.value(QStringLiteral("energy"));
        if (!object.value(QStringLiteral("time")).isString()
            || !energy.isDouble() || !std::isfinite(energy.toDouble())
            || energy.toDouble() < 0.0) {
            emit requestFailed(route, static_cast<int>(BusinessErrorCode::InvalidArgument),
                               QStringLiteral("服务器响应格式错误"), 0);
            return;
        }
        bundle.actual.append({object.value(QStringLiteral("station_id")).toInteger(-1),
                              object.value(QStringLiteral("station_name")).toString(),
                              object.value(QStringLiteral("time")).toString(),
                              energy.toDouble()});
    }    bundle.runInProgress =
        response.data.value(QStringLiteral("running")).toBool(false);
    bundle.lastRunError =
        response.data.value(QStringLiteral("last_error")).toString();    emit predictionsReceived(bundle);
}

}
