// UC-A-08 智能预测页图表构建（NFR-M-01：从 predict_page.cpp 拆分，保持 ≤400 行）
#include "predict_page.h"

#include "dashboard_ui.h"

#include <QComboBox>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QCategoryAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>
#include <QColor>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QPainter>
#include <algorithm>

namespace ncs {
namespace {

// 统一为 "yyyy-MM-dd HH:00"，兼容 SQLite 与 Python 两种时间字符串长度
QString hourKey(const QString &time)
{
    return time.left(16);
}

QString shortLabel(const QString &key)
{
    // "2026-09-07 14:00" → "09-07 14时"
    return QStringLiteral("%1 %2时").arg(key.mid(5, 5), key.mid(11, 2));
}

}

void PredictPage::setupChartCard(QGridLayout *grid)
{
    auto *card = dashboard::card(this);
    card->setMinimumHeight(400);
    auto *layout = dashboard::cardLayout(card);
    dashboard::addCardHeader(layout, tr("历史实际与模型预测对比"),
                             tr("实际负荷来自已完成订单，预测负荷来自模型输出"));

    auto *filterRow = new QHBoxLayout;
    filterRow->setSpacing(8);
    stationFilter_ = new QComboBox(card);
    stationFilter_->setObjectName("predictionStationFilter");
    horizonFilter_ = new QComboBox(card);
    horizonFilter_->setObjectName("predictionHorizonFilter");
    horizonFilter_->addItem(tr("全部周期"), -1);
    horizonFilter_->addItem(tr("未来 1 小时"), 1);
    horizonFilter_->addItem(tr("未来 6 小时"), 6);
    horizonFilter_->addItem(tr("未来 24 小时"), 24);
    filterRow->addWidget(new QLabel(tr("站点"), card));
    filterRow->addWidget(stationFilter_, 1);
    filterRow->addWidget(new QLabel(tr("周期"), card));
    filterRow->addWidget(horizonFilter_);
    layout->addLayout(filterRow);

    statusNote_ = new QLabel(tr("暂无数据"), card);
    statusNote_->setObjectName("trendStatus");
    layout->addWidget(statusNote_);

    auto *chart = new QChart;
    chart->setBackgroundBrush(QColor(QStringLiteral("#FFFFFF")));
    chart->setBackgroundRoundness(16);
    chart->setPlotAreaBackgroundVisible(true);
    chart->setPlotAreaBackgroundBrush(QColor(QStringLiteral("#FCFBFF")));
    actualSeries_ = new QLineSeries;
    actualSeries_->setName(tr("历史实际负荷"));
    actualSeries_->setPen(QPen(QColor(QStringLiteral("#8A94A6")), 2.2));
    predictedSeries_ = new QLineSeries;
    predictedSeries_->setName(tr("模型预测负荷"));
    predictedSeries_->setPen(QPen(QColor(QStringLiteral("#1E88E5")), 2.5));
    predictedSeries_->setPointsVisible(true);
    peakSeries_ = new QScatterSeries;
    peakSeries_->setName(tr("高峰时段"));
    peakSeries_->setMarkerShape(QScatterSeries::MarkerShapeCircle);
    peakSeries_->setMarkerSize(11);
    peakSeries_->setColor(QColor(QStringLiteral("#E53935")));
    peakSeries_->setBorderColor(QColor(QStringLiteral("#E53935")));
    chart->addSeries(actualSeries_);
    chart->addSeries(predictedSeries_);
    chart->addSeries(peakSeries_);
    timeAxis_ = new QCategoryAxis;
    timeAxis_->setLabelsPosition(QCategoryAxis::AxisLabelsPositionOnValue);
    energyAxis_ = new QValueAxis;
    energyAxis_->setTitleText(tr("充电量（度）"));
    energyAxis_->setLabelFormat(QStringLiteral("%.1f"));
    energyAxis_->setRange(0, 1);
    chart->addAxis(timeAxis_, Qt::AlignBottom);
    chart->addAxis(energyAxis_, Qt::AlignLeft);
    actualSeries_->attachAxis(timeAxis_);
    actualSeries_->attachAxis(energyAxis_);
    predictedSeries_->attachAxis(timeAxis_);
    predictedSeries_->attachAxis(energyAxis_);
    peakSeries_->attachAxis(timeAxis_);
    peakSeries_->attachAxis(energyAxis_);
    chartView_ = new QChartView(chart, card);
    chartView_->setObjectName("predictionChart");
    chartView_->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(chartView_);

    grid->addWidget(card, 1, 0, 1, 10);

    connect(stationFilter_, &QComboBox::currentIndexChanged, this,
            [this] { rebuildChart(); rebuildTable(); });
    connect(horizonFilter_, &QComboBox::currentIndexChanged, this,
            [this] { rebuildChart(); rebuildTable(); });
}

void PredictPage::rebuildChart()
{
    if (!stationFilter_ || !actualSeries_ || !predictedSeries_ || !peakSeries_
        || !timeAxis_ || !energyAxis_ || !chartView_ || !statusNote_) {
        return;
    }
    // 仅展示逐小时曲线（horizon=24），汇总行（1/6 小时）不进入曲线
    // "全部站点" 项 userData 为空串，必须用 currentData 而非 currentText 判断
    const QString station = stationFilter_->currentData().toString();
    QMap<QString, QPair<double, double>> timeline;  // key → (actual, predicted)
    for (const HourlyLoad &point : bundle_.actual) {
        // 实际负荷按站点聚合，切换站点时灰色曲线同步过滤
        if (!station.isEmpty() && point.stationName != station) {
            continue;
        }
        timeline[hourKey(point.time)].first += point.energy;
    }
    for (const LoadPrediction &prediction : bundle_.items) {
        if (prediction.horizonHours != 24
            || (!station.isEmpty() && prediction.stationName != station)) {
            continue;
        }
        timeline[hourKey(prediction.targetTime)].second += prediction.predictedEnergy;
    }

    actualSeries_->clear();
    predictedSeries_->clear();
    peakSeries_->clear();
    for (const QString &label : timeAxis_->categoriesLabels()) {
        timeAxis_->remove(label);
    }

    const int count = timeline.size();
    if (count == 0) {
        chartView_->hide();
        statusNote_->setText(tr("暂无预测数据，请点击「运行预测」生成"));
        energyAxis_->setRange(0, 1);
        return;
    }
    chartView_->show();
    statusNote_->setText(tr("共 %1 条逐小时预测记录").arg(timeline.size()));

    double maxEnergy = 0.0;
    int index = 0;
    const int labelStride = std::max(1, count / 12);
    timeAxis_->setRange(0, std::max(1, count - 1));
    for (auto it = timeline.constBegin(); it != timeline.constEnd(); ++it, ++index) {
        if (it->first >= 0.0) {
            actualSeries_->append(index, it->first);
        }
        if (it->second >= 0.0) {
            predictedSeries_->append(index, it->second);
        }
        maxEnergy = std::max({maxEnergy, it->first, it->second});
        if (index % labelStride == 0) {
            timeAxis_->append(shortLabel(it.key()), index);
        }
    }
    // 高峰点单独散点标注（红色高亮）
    for (const LoadPrediction &prediction : bundle_.items) {
        if (prediction.horizonHours != 24 || !prediction.isPeak
            || (!station.isEmpty() && prediction.stationName != station)) {
            continue;
        }
        const QString key = hourKey(prediction.targetTime);
        if (!timeline.contains(key)) {
            continue;
        }
        const int position = static_cast<int>(std::distance(
            timeline.constBegin(), timeline.constFind(key)));
        if (position >= 0) {
            peakSeries_->append(position, prediction.predictedEnergy);
        }
    }
    energyAxis_->setRange(0, std::max(1.0, maxEnergy * 1.15));
}

}  // namespace ncs
