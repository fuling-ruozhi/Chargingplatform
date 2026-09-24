#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>

namespace ncs {

// load_prediction 表的一行（UC-M-03 契约），关联站名便于展示
struct LoadPrediction
{
    qint64 id = 0;
    qint64 stationId = 0;
    QString stationName;
    QString generatedAt;
    QString targetTime;
    int horizonHours = 24;
    double predictedEnergy = 0.0;
    int predictedFreeChargers = 0;
    bool isPeak = false;
};

using PredictionList = QVector<LoadPrediction>;

// 历史实际逐小时负荷（来自 charging_order 聚合，用于与预测曲线对比）
struct HourlyLoad
{
    qint64 stationId = -1;  // -1 表示全平台聚合（未指定站点）
    QString stationName;
    QString time;           // "yyyy-MM-dd HH:00" 本地时间（BR-09）
    double energy = 0.0;
};

struct PredictionBundle
{
    QString generatedAt;             // 最近一次预测生成时间（数据截止时间）
    PredictionList items;
    QVector<HourlyLoad> actual;      // 近 24 小时实际负荷
    bool runInProgress = false;      // UC-A-08：预测脚本是否正在后台运行（供轮询）
    QString lastRunError;            // 上一次脚本运行的失败原因（空表示无错误）
};

}

Q_DECLARE_METATYPE(ncs::LoadPrediction)
Q_DECLARE_METATYPE(ncs::PredictionList)
Q_DECLARE_METATYPE(ncs::PredictionBundle)
