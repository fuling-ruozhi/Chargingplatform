#pragma once

#include "model/charger_status_summary.h"

#include <QWidget>

class QLabel;
class QProgressBar;
class QChartView;
class QPieSeries;

namespace ncs {

class AdminClientFacade;

class ChargerStatusPage : public QWidget
{
    Q_OBJECT
public:
    explicit ChargerStatusPage(QWidget *parent = nullptr);
    void bindFacade(AdminClientFacade &facade);
    void refresh();
    void applySummary(const ChargerStatusSummary &summary);

protected:
    void showEvent(QShowEvent *event) override;

private:
    AdminClientFacade *facade_ = nullptr;
    bool loaded_ = false;
    QLabel *metrics_[3]{};
    QLabel *health_ = nullptr;
    QLabel *note_ = nullptr;
    QProgressBar *progress_ = nullptr;
    QPieSeries *pie_ = nullptr;
    QChartView *view_ = nullptr;
};

}
