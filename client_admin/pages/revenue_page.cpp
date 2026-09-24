#include "revenue_page.h"

#include "dashboard_ui.h"
#include "service/admin_client_facade.h"

#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QCategoryAxis>
#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QtCharts/QLineSeries>
#include <QScrollArea>
#include <QShowEvent>
#include <QTableWidget>
#include <QtCharts/QValueAxis>
#include <QVBoxLayout>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>

#include <cmath>

namespace ncs {

RevenuePage::RevenuePage(QWidget *parent) : QWidget(parent)
{
    auto *content = new QWidget;
    content->setObjectName(QStringLiteral("pageCanvas"));
    auto *grid = new QGridLayout(content);
    grid->setContentsMargins(0, 0, 4, 8);
    grid->setSpacing(16);

    auto *trend = dashboard::card(content);
    trend->setMinimumHeight(410);
    auto *trendLayout = dashboard::cardLayout(trend);
    dashboard::addCardHeader(trendLayout, tr("营收趋势"), tr("按结算日期统计"));
    range_ = new QComboBox(trend);
    range_->setObjectName(QStringLiteral("revenueRange"));
    range_->addItem(tr("最近 7 天"), 7);
    range_->addItem(tr("最近 30 天"), 30);
    trendLayout->addWidget(range_, 0, Qt::AlignRight);
    trendNote_ = new QLabel(tr("暂无数据"), trend);
    trendNote_->setObjectName(QStringLiteral("trendStatus"));
    trendLayout->addWidget(trendNote_);

    auto *lineChart = new QChart;
    lineChart->setBackgroundBrush(QColor(QStringLiteral("#FFFFFF")));
    lineChart->setBackgroundRoundness(16);
    lineChart->setPlotAreaBackgroundVisible(true);
    lineChart->setPlotAreaBackgroundBrush(QColor(QStringLiteral("#FCFBFF")));
    line_ = new QLineSeries;
    line_->setName(tr("营收"));
    line_->setPen(QPen(QColor(QStringLiteral("#8276C9")), 2.5));
    line_->setPointsVisible(true);
    lineChart->addSeries(line_);
    lineDates_ = new QCategoryAxis;
    lineDates_->setLabelsPosition(QCategoryAxis::AxisLabelsPositionOnValue);
    revenueAxis_ = new QValueAxis;
    revenueAxis_->setTitleText(tr("营收（元）"));
    revenueAxis_->setLabelFormat(QStringLiteral("%.2f"));
    revenueAxis_->setRange(0, 1);
    lineChart->addAxis(lineDates_, Qt::AlignBottom);
    lineChart->addAxis(revenueAxis_, Qt::AlignLeft);
    line_->attachAxis(lineDates_);
    line_->attachAxis(revenueAxis_);
    lineView_ = new QChartView(lineChart, trend);
    lineView_->setObjectName(QStringLiteral("revenueLineChart"));
    lineView_->setRenderHint(QPainter::Antialiasing);
    trendLayout->addWidget(lineView_);
    grid->addWidget(trend, 0, 0, 3, 7);

    const QString titles[] = {tr("今日营收"), tr("本月营收"), tr("累计营收")};
    const QString styles[] = {QStringLiteral("darkMetricCard"),
                              QStringLiteral("purpleMetricCard"),
                              QStringLiteral("warmMetricCard")};
    for (int index = 0; index < 3; ++index) {
        auto *metric = dashboard::metricCard(
            content, titles[index], QStringLiteral("--"), tr("仅已完成记录"), styles[index]);
        metrics_[index] = metric->findChild<QLabel *>(QStringLiteral("metricValue"));
        metrics_[index]->setProperty(
            "metricKey", QStringList{QStringLiteral("today"), QStringLiteral("month"),
                                     QStringLiteral("total")}[index]);
        if (index == 0) {
            summaryNote_ = metric->findChild<QLabel *>(QStringLiteral("metricNote"));
        }
        grid->addWidget(metric, index, 7, 1, 3);
    }

    auto *counts = dashboard::card(content);
    counts->setMinimumHeight(360);
    auto *countsLayout = dashboard::cardLayout(counts);
    dashboard::addCardHeader(countsLayout, tr("每日完成订单数"),
                             tr("与营收趋势使用相同日期范围"));
    barNote_ = new QLabel(tr("暂无数据"), counts);
    barNote_->setObjectName(QStringLiteral("orderCountStatus"));
    countsLayout->addWidget(barNote_);
    auto *barChart = new QChart;
    barChart->setBackgroundBrush(QColor(QStringLiteral("#FFFFFF")));
    barChart->setBackgroundRoundness(16);
    barChart->setPlotAreaBackgroundVisible(true);
    barChart->setPlotAreaBackgroundBrush(QColor(QStringLiteral("#FCFBFF")));
    auto *barSeries = new QBarSeries;
    bars_ = new QBarSet(tr("已完成"));
    bars_->setColor(QColor(QStringLiteral("#A59ACF")));
    bars_->setBorderColor(Qt::transparent);
    barSeries->append(bars_);
    barChart->addSeries(barSeries);
    barDates_ = new QBarCategoryAxis;
    countAxis_ = new QValueAxis;
    countAxis_->setLabelFormat(QStringLiteral("%.0f"));
    countAxis_->setRange(0, 1);
    countAxis_->setTitleText(tr("订单数"));
    barChart->addAxis(barDates_, Qt::AlignBottom);
    barChart->addAxis(countAxis_, Qt::AlignLeft);
    barSeries->attachAxis(barDates_);
    barSeries->attachAxis(countAxis_);
    barView_ = new QChartView(barChart, counts);
    barView_->setObjectName(QStringLiteral("orderBarChart"));
    barView_->setRenderHint(QPainter::Antialiasing);
    countsLayout->addWidget(barView_);
    grid->addWidget(counts, 3, 0, 1, 10);

    auto *recent = dashboard::card(content, QStringLiteral("tableCard"));
    recent->setMinimumHeight(320);
    auto *recentLayout = dashboard::cardLayout(recent);
    dashboard::addCardHeader(recentLayout, tr("最近订单"),
                             tr("最近 10 条已完成充电记录"));
    ordersNote_ = new QLabel(tr("暂无数据"), recent);
    ordersNote_->setObjectName(QStringLiteral("ordersStatus"));
    recentLayout->addWidget(ordersNote_);
    orders_ = new QTableWidget(0, 6, recent);
    orders_->setObjectName(QStringLiteral("recentOrdersTable"));
    orders_->setHorizontalHeaderLabels(
        {tr("记录 ID"), tr("电桩"), tr("站点"), tr("开始时间"), tr("金额"), tr("状态")});
    orders_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    orders_->verticalHeader()->hide();
    orders_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    orders_->setSelectionBehavior(QAbstractItemView::SelectRows);
    orders_->setShowGrid(false);
    orders_->setFrameShape(QFrame::NoFrame);
    orders_->setMinimumHeight(220);
    recentLayout->addWidget(orders_);
    grid->addWidget(recent, 4, 0, 1, 10);
    grid->setColumnStretch(0, 7);
    grid->setColumnStretch(7, 3);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(dashboard::scrollPage(content, this));
    connect(range_, &QComboBox::currentIndexChanged, this, [this] { loadTrend(); });
}

void RevenuePage::bindFacade(AdminClientFacade &facade)
{
    if (facade_) {
        return;
    }
    facade_ = &facade;
    connect(&facade, &AdminClientFacade::revenueSummaryReceived,
            this, &RevenuePage::applySummary);
    connect(&facade, &AdminClientFacade::revenueTrendReceived,
            this, &RevenuePage::applyTrend);
    connect(&facade, &AdminClientFacade::recentOrdersReceived,
            this, &RevenuePage::applyOrders);
    connect(&facade, &AdminClientFacade::requestFailed, this,
            [this](const QString &route, int, const QString &message, int) {
                const QString text = tr("数据加载失败：%1").arg(message);
                if (route == QStringLiteral("admin.revenue.summary")) {
                    for (auto *metric : metrics_) {
                        metric->setText(QStringLiteral("--"));
                    }
                    summaryNote_->setText(text);
                } else if (route == QStringLiteral("admin.revenue.trend")) {
                    trendNote_->setText(text);
                    barNote_->setText(text);
                } else if (route == QStringLiteral("admin.revenue.recentOrders")) {
                    ordersNote_->setText(text);
                }
            });
    if (isVisible() && !loaded_) {
        refresh();
    }
}

void RevenuePage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (facade_ && !loaded_) {
        refresh();
    }
}

void RevenuePage::refresh()
{
    if (!facade_) {
        return;
    }
    loaded_ = true;
    for (auto *metric : metrics_) {
        metric->setText(QStringLiteral("..."));
    }
    summaryNote_->setText(tr("加载中"));
    orders_->setRowCount(0);
    ordersNote_->setText(tr("加载中"));
    facade_->requestRevenueSummary();
    loadTrend();
    facade_->requestRecentOrders();
}

void RevenuePage::loadTrend()
{
    lineView_->hide();
    barView_->hide();
    const QString state = facade_ ? tr("加载中") : tr("暂无数据");
    trendNote_->setText(state);
    barNote_->setText(state);
    if (facade_) {
        facade_->requestRevenueTrend(range_->currentData().toInt());
    }
}

void RevenuePage::applySummary(const RevenueSummary &summary)
{
    const double values[] = {summary.todayRevenue, summary.monthRevenue,
                             summary.totalRevenue};
    for (int index = 0; index < 3; ++index) {
        metrics_[index]->setText(QStringLiteral("￥%1").arg(values[index], 0, 'f', 2));
    }
    summaryNote_->setText(tr("仅已完成记录"));
}

void RevenuePage::applyTrend(const RevenueTrend &trend)
{
    if (trend.days != range_->currentData().toInt()) {
        return;
    }
    line_->clear();
    bars_->remove(0, bars_->count());
    for (const auto &label : lineDates_->categoriesLabels()) {
        lineDates_->remove(label);
    }
    QStringList dates;
    double maxRevenue = 0.0;
    qint64 maxCount = 0;
    qint64 totalCount = 0;
    for (int index = 0; index < trend.items.size(); ++index) {
        const auto &day = trend.items.at(index);
        const QString date = day.date.toString(QStringLiteral("MM-dd"));
        line_->append(index, day.revenue);
        *bars_ << day.orderCount;
        dates.append(date);
        if (trend.days == 7 || index % 5 == 0 || index == trend.items.size() - 1) {
            lineDates_->append(date, index);
        }
        maxRevenue = qMax(maxRevenue, day.revenue);
        maxCount = qMax(maxCount, day.orderCount);
        totalCount += day.orderCount;
    }
    lineDates_->setRange(-0.5, trend.days - 0.5);
    barDates_->clear();
    barDates_->append(dates);
    barDates_->setLabelsAngle(trend.days == 30 ? -60 : 0);
    revenueAxis_->setRange(0, qMax(1.0, maxRevenue * 1.15));
    const double step = qMax(1.0, std::ceil(maxCount / 4.0));
    countAxis_->setRange(0, step * 4);
    countAxis_->setTickCount(5);
    const QString state = totalCount > 0
        ? tr("仅统计已完成记录") : tr("暂无数据（日期已补零）");
    trendNote_->setText(state);
    barNote_->setText(state);
    lineView_->show();
    barView_->show();
}

void RevenuePage::applyOrders(const RecentOrders &orders)
{
    orders_->setRowCount(orders.size());
    for (int row = 0; row < orders.size(); ++row) {
        const auto &order = orders.at(row);
        const auto start = QDateTime::fromString(order.startTime, Qt::ISODateWithMs)
                               .toLocalTime();
        const QStringList cells{
            QString::number(order.id), order.chargerCode, order.stationName,
            start.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
            QStringLiteral("￥%1").arg(order.cost, 0, 'f', 2), tr("已完成")};
        for (int column = 0; column < cells.size(); ++column) {
            orders_->setItem(row, column, new QTableWidgetItem(cells.at(column)));
        }
    }
    ordersNote_->setText(orders.isEmpty()
        ? tr("暂无数据") : tr("最近 %1 条已完成记录").arg(orders.size()));
}

}
