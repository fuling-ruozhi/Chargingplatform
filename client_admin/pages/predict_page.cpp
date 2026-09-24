#include "predict_page.h"

#include "dashboard_ui.h"
#include "service/admin_client_facade.h"

#include <QColor>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QShowEvent>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace ncs {

PredictPage::PredictPage(QWidget *parent) : QWidget(parent)
{
    auto *content = new QWidget;
    content->setObjectName("pageCanvas");

    auto *grid = new QGridLayout(content);
    grid->setContentsMargins(0, 0, 4, 8);
    grid->setSpacing(16);

    setupHero(grid);
    setupChartCard(grid);
    setupTableCard(grid);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(dashboard::scrollPage(content, this));
}

void PredictPage::setupHero(QGridLayout *grid)
{
    auto *hero = dashboard::card(this, "predictionCard");
    hero->setMinimumHeight(170);
    hero->setMaximumHeight(210);
    auto *heroLayout = dashboard::cardLayout(hero, 32, 8);

    auto *eyebrow = new QLabel(tr("NCS 智能预测"), hero);
    eyebrow->setObjectName("predictionEyebrow");
    auto *title = new QLabel(tr("智能负载预测"), hero);
    title->setObjectName("predictionTitle");
    auto *description = new QLabel(tr("基于历史充电负荷，调用机器学习模型预测未来负载"),
                                   hero);
    description->setObjectName("predictionSubtitle");
    heroLayout->addWidget(eyebrow);
    heroLayout->addWidget(title);
    heroLayout->addWidget(description);

    auto *bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(16);
    generatedLabel_ = new QLabel(tr("暂无预测数据"), hero);
    generatedLabel_->setObjectName("secondaryText");
    bottomRow->addWidget(generatedLabel_);
    bottomRow->addStretch();
    runButton_ = new QPushButton(tr("运行预测"), hero);
    runButton_->setObjectName("predictionRunButton");
    bottomRow->addWidget(runButton_);
    heroLayout->addLayout(bottomRow);

    grid->addWidget(hero, 0, 0, 1, 10);
}

void PredictPage::setupTableCard(QGridLayout *grid)
{
    auto *card = dashboard::card(this, QStringLiteral("tableCard"));
    card->setMinimumHeight(320);
    auto *layout = dashboard::cardLayout(card);
    dashboard::addCardHeader(layout, tr("预测明细"),
                             tr("高峰时段以红色标注，作为负荷预警"));
    table_ = new QTableWidget(0, 6, card);
    table_->setObjectName("predictionTable");
    table_->setHorizontalHeaderLabels({tr("电站"), tr("目标时间"), tr("周期"),
                                       tr("预测充电量(度)"), tr("预测空闲桩数"),
                                       tr("高峰")});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->verticalHeader()->hide();
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setShowGrid(false);
    table_->setFrameShape(QFrame::NoFrame);
    table_->setMinimumHeight(240);
    layout->addWidget(table_);

    grid->addWidget(card, 2, 0, 1, 10);
}

void PredictPage::bindFacade(AdminClientFacade &facade)
{
    if (facade_) {
        return;
    }
    facade_ = &facade;
    connect(&facade, &AdminClientFacade::predictionsReceived,
            this, &PredictPage::applyPredictions);
    connect(&facade, &AdminClientFacade::predictionRunSucceeded, this, [this] {
        // 请求已被服务器接受，脚本开始异步执行；开始轮询跟踪结果
        running_ = true;
        if (runButton_) {
            runButton_->setEnabled(false);
        }
        if (statusNote_) {
            statusNote_->setText(tr("预测脚本已开始运行，结果生成后自动刷新…"));
        }
        startRunPolling();
    });
    connect(&facade, &AdminClientFacade::requestFailed, this,
            [this](const QString &route, int, const QString &message, int) {
                if (route == QStringLiteral("admin.prediction.run")) {
                    running_ = false;
                    if (runButton_) {
                        runButton_->setEnabled(true);
                    }
                    stopRunPolling();
                    if (statusNote_) {
                        statusNote_->setText(tr("预测请求失败：%1").arg(message));
                    }
                } else if (route == QStringLiteral("admin.prediction.list")) {
                    if (statusNote_) {
                        statusNote_->setText(tr("数据加载失败：%1").arg(message));
                    }
                }
            });
    connect(runButton_, &QPushButton::clicked, this, [this] {
        if (!facade_ || running_) {
            return;
        }
        running_ = true;
        if (runButton_) {
            runButton_->setEnabled(false);
        }
        if (statusNote_) {
            statusNote_->setText(tr("正在请求运行预测…"));
        }
        facade_->runPrediction();
    });
    if (isVisible() && !loaded_) {
        refresh();
    }
}

void PredictPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (facade_ && !loaded_) {
        refresh();
    }
}

void PredictPage::refresh()
{
    if (!facade_) {
        return;
    }
    loaded_ = true;
    if (statusNote_) {
        statusNote_->setText(tr("加载中"));
    }
    facade_->requestPredictions();
}

void PredictPage::applyPredictions(const PredictionBundle &bundle)
{
    bundle_ = bundle;
    // 轮询响应：脚本仍在运行则继续轮询；一旦结束展示结果或失败原因
    bool runJustFinished = false;
    if (bundle.runInProgress) {
        running_ = true;
        if (runButton_) {
            runButton_->setEnabled(false);
        }
        startRunPolling();
    } else {
        const bool wasAwaiting = pollTimer_ && pollTimer_->isActive();
        stopRunPolling();
        runJustFinished = wasAwaiting || running_;
        if (runJustFinished) {
            running_ = false;
            if (runButton_) {
                runButton_->setEnabled(true);
            }
        }
    }
    rebuildFilters();
    rebuildChart();
    rebuildTable();
    if (generatedLabel_) {
        generatedLabel_->setText(bundle.generatedAt.isEmpty()
            ? tr("暂无预测数据，请点击「运行预测」生成")
            : tr("数据生成时间：%1").arg(bundle.generatedAt));
    }
    // 状态提示需晚于 rebuildChart 写入，否则会被其默认文案覆盖
    if (bundle.runInProgress) {
        if (statusNote_) {
            statusNote_->setText(tr("预测脚本运行中，结果生成后自动刷新…"));
        }
    } else if (runJustFinished && statusNote_) {
        statusNote_->setText(bundle.lastRunError.isEmpty()
            ? tr("预测完成，已更新最新结果")
            : tr("预测执行失败：%1（已保留上次结果）").arg(bundle.lastRunError));
    }
}

void PredictPage::startRunPolling()
{
    if (!pollTimer_) {
        pollTimer_ = new QTimer(this);
        connect(pollTimer_, &QTimer::timeout, this, [this] {
            if (facade_) {
                facade_->requestPredictions();
            }
        });
    }
    if (!pollTimer_->isActive()) {
        pollTimer_->start(3'000);
    }
}

void PredictPage::stopRunPolling()
{
    if (pollTimer_) {
        pollTimer_->stop();
    }
}

void PredictPage::rebuildFilters()
{
    if (!stationFilter_) {
        return;
    }
    QSet<QString> stations;
    for (const LoadPrediction &prediction : bundle_.items) {
        stations.insert(prediction.stationName);
    }
    QStringList names = stations.values();
    names.sort();
    const QString previous = stationFilter_->currentText();
    QSignalBlocker blocker(stationFilter_);
    stationFilter_->clear();
    stationFilter_->addItem(tr("全部站点"), QString());
    // 每项必须显式写入 userData，否则 currentData() 过滤失效
    for (const QString &name : names) {
        stationFilter_->addItem(name, name);
    }
    const int index = stationFilter_->findText(previous);
    if (index >= 0) {
        stationFilter_->setCurrentIndex(index);
    }
}

void PredictPage::rebuildTable()
{
    if (!table_) {
        return;
    }
    const PredictionList rows = visiblePredictions();
    table_->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row) {
        const LoadPrediction &prediction = rows.at(row);
        const QString horizon = prediction.horizonHours == 1
            ? tr("未来 1 小时") : (prediction.horizonHours == 6
            ? tr("未来 6 小时") : tr("未来 24 小时"));
        const QStringList cells = {prediction.stationName, prediction.targetTime,
                                   horizon,
                                   QString::number(prediction.predictedEnergy, 'f', 2),
                                   QString::number(prediction.predictedFreeChargers),
                                   prediction.isPeak ? tr("高峰") : QStringLiteral("--")};
        for (int column = 0; column < cells.size(); ++column) {
            auto *item = new QTableWidgetItem(cells.at(column));
            if (prediction.isPeak) {
                item->setForeground(QColor(QStringLiteral("#E53935")));
            }
            table_->setItem(row, column, item);
        }
    }
}

PredictionList PredictPage::visiblePredictions() const
{
    const QString station = stationFilter_
        ? stationFilter_->currentData().toString() : QString();
    const int horizon =
        horizonFilter_ ? horizonFilter_->currentData().toInt() : -1;
    PredictionList rows;
    for (const LoadPrediction &prediction : bundle_.items) {
        if (!station.isEmpty() && prediction.stationName != station) {
            continue;
        }
        if (horizon >= 0 && prediction.horizonHours != horizon) {
            continue;
        }
        rows.append(prediction);
    }
    return rows;
}

}  // namespace ncs
