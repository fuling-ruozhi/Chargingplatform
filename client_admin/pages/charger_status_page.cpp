#include "charger_status_page.h"

#include "dashboard_ui.h"
#include "service/admin_client_facade.h"

#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QProgressBar>
#include <QScrollArea>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>

namespace ncs {

ChargerStatusPage::ChargerStatusPage(QWidget *parent) : QWidget(parent)
{
    auto *content = new QWidget;
    content->setObjectName(QStringLiteral("pageCanvas"));
    auto *grid = new QGridLayout(content);
    grid->setContentsMargins(0, 0, 4, 8);
    grid->setSpacing(16);

    const QString titles[] = {tr("空闲电桩"), tr("使用中"), tr("故障")};
    const QString styles[] = {QStringLiteral("card"), QStringLiteral("purpleMetricCard"),
                              QStringLiteral("warmMetricCard")};
    for (int index = 0; index < 3; ++index) {
        auto *metric = dashboard::metricCard(
            content, titles[index], QStringLiteral("--"), tr("当前设备状态"), styles[index]);
        metric->setMaximumHeight(112);
        metrics_[index] = metric->findChild<QLabel *>(QStringLiteral("metricValue"));
        metrics_[index]->setProperty(
            "metricKey", QStringList{QStringLiteral("idle"), QStringLiteral("inUse"),
                                     QStringLiteral("fault")}[index]);
        grid->addWidget(metric, 0, index);
    }

    auto *distribution = dashboard::card(content);
    distribution->setMinimumHeight(380);
    auto *distributionLayout = dashboard::cardLayout(distribution);
    dashboard::addCardHeader(distributionLayout, tr("电桩状态分布"),
                             tr("空闲、使用中和故障设备占比"));
    note_ = new QLabel(tr("暂无电桩数据"), distribution);
    note_->setObjectName(QStringLiteral("chargerStatusNote"));
    distributionLayout->addWidget(note_);

    auto *chart = new QChart;
    chart->setBackgroundBrush(QColor(QStringLiteral("#FFFFFF")));
    chart->setBackgroundRoundness(16);
    chart->setPlotAreaBackgroundVisible(true);
    chart->setPlotAreaBackgroundBrush(QColor(QStringLiteral("#FCFBFF")));
    pie_ = new QPieSeries;
    pie_->setHoleSize(0.64);
    chart->addSeries(pie_);
    view_ = new QChartView(chart, distribution);
    view_->setObjectName(QStringLiteral("chargerPieChart"));
    view_->setRenderHint(QPainter::Antialiasing);
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);
    distributionLayout->addWidget(view_, 1);
    grid->addWidget(distribution, 1, 0, 1, 2);

    auto *healthCard = dashboard::card(content, QStringLiteral("darkMetricCard"));
    auto *healthLayout = dashboard::cardLayout(healthCard, 24, 14);
    dashboard::addCardHeader(healthLayout, tr("平台健康度"),
                             tr("空闲与使用中电桩占比"));
    health_ = new QLabel(QStringLiteral("--%"), healthCard);
    health_->setObjectName(QStringLiteral("healthValue"));
    healthLayout->addWidget(health_);
    progress_ = new QProgressBar(healthCard);
    progress_->setObjectName(QStringLiteral("healthProgress"));
    progress_->setRange(0, 100);
    progress_->setValue(0);
    progress_->setTextVisible(false);
    healthLayout->addWidget(progress_);
    healthLayout->addStretch();
    grid->addWidget(healthCard, 1, 2);

    for (int index = 0; index < 3; ++index) {
        grid->setColumnStretch(index, 1);
    }
    grid->setRowStretch(1, 1);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(dashboard::scrollPage(content, this));
}

void ChargerStatusPage::bindFacade(AdminClientFacade &facade)
{
    if (facade_) {
        return;
    }
    facade_ = &facade;
    connect(&facade, &AdminClientFacade::chargerStatusReceived,
            this, &ChargerStatusPage::applySummary);
    connect(&facade, &AdminClientFacade::requestFailed, this,
            [this](const QString &route, int, const QString &message, int) {
                if (route != QStringLiteral("admin.charger.statusSummary")) {
                    return;
                }
                for (auto *metric : metrics_) {
                    metric->setText(QStringLiteral("--"));
                }
                health_->setText(QStringLiteral("--%"));
                progress_->setValue(0);
                view_->hide();
                note_->setText(tr("数据加载失败：%1").arg(message));
            });
    if (isVisible() && !loaded_) {
        refresh();
    }
}

void ChargerStatusPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (facade_ && !loaded_) {
        refresh();
    }
}

void ChargerStatusPage::refresh()
{
    if (!facade_) {
        return;
    }
    loaded_ = true;
    for (auto *metric : metrics_) {
        metric->setText(QStringLiteral("..."));
    }
    health_->setText(QStringLiteral("..."));
    progress_->setValue(0);
    view_->hide();
    note_->setText(tr("加载中"));
    facade_->requestChargerStatusSummary();
}

void ChargerStatusPage::applySummary(const ChargerStatusSummary &summary)
{
    const qint64 counts[] = {summary.idle, summary.inUse, summary.fault};
    const QString titles[] = {tr("空闲"), tr("使用中"), tr("故障")};
    const QColor colors[] = {QColor(QStringLiteral("#93B5A0")),
                             QColor(QStringLiteral("#9586C5")),
                             QColor(QStringLiteral("#D49D96"))};
    pie_->clear();
    for (int index = 0; index < 3; ++index) {
        metrics_[index]->setText(QString::number(counts[index]));
        auto *slice = pie_->append(
            QStringLiteral("%1 %2").arg(titles[index]).arg(counts[index]), counts[index]);
        slice->setColor(colors[index]);
        slice->setBorderColor(Qt::transparent);
    }
    const double health = summary.total > 0
        ? qBound(0.0, summary.health, 100.0) : 0.0;
    health_->setText(summary.total > 0
        ? QStringLiteral("%1%").arg(health, 0, 'f', 1) : QStringLiteral("0%"));
    progress_->setValue(qRound(health));
    note_->setText(summary.total > 0
        ? tr("共 %1 台电桩").arg(summary.total) : tr("暂无电桩数据"));
    view_->show();
}

}
