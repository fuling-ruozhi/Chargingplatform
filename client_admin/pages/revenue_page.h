#pragma once

#include "model/revenue_stats.h"

#include <QWidget>

class QLabel;
class QComboBox;
class QTableWidget;
class QChartView;
class QBarCategoryAxis;
class QBarSet;
class QCategoryAxis;
class QLineSeries;
class QValueAxis;

namespace ncs {

class AdminClientFacade;

class RevenuePage : public QWidget
{
    Q_OBJECT
public:
    explicit RevenuePage(QWidget *parent = nullptr);
    void bindFacade(AdminClientFacade &facade);
    void refresh();
    void applySummary(const RevenueSummary &summary);
    void applyTrend(const RevenueTrend &trend);
    void applyOrders(const RecentOrders &orders);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void loadTrend();

    AdminClientFacade *facade_ = nullptr;
    bool loaded_ = false;
    QLabel *metrics_[3]{};
    QLabel *summaryNote_ = nullptr;
    QLabel *trendNote_ = nullptr;
    QLabel *barNote_ = nullptr;
    QLabel *ordersNote_ = nullptr;
    QComboBox *range_ = nullptr;
    QTableWidget *orders_ = nullptr;
    QChartView *lineView_ = nullptr;
    QChartView *barView_ = nullptr;
    QLineSeries *line_ = nullptr;
    QBarSet *bars_ = nullptr;
    QCategoryAxis *lineDates_ = nullptr;
    QBarCategoryAxis *barDates_ = nullptr;
    QValueAxis *revenueAxis_ = nullptr;
    QValueAxis *countAxis_ = nullptr;
};

}
