#pragma once

#include "model/load_prediction.h"

#include <QWidget>

class QLabel;
class QComboBox;
class QGridLayout;
class QPushButton;
class QTableWidget;
class QTimer;
class QChartView;
class QCategoryAxis;
class QLineSeries;
class QScatterSeries;
class QValueAxis;

namespace ncs {

class AdminClientFacade;

// UC-A-08 智能预测展示：读取 load_prediction 表并支持触发 Python 预测脚本。
// 脚本在服务器异步运行，本页通过轮询 admin.prediction.list 跟踪进度。
class PredictPage : public QWidget
{
    Q_OBJECT
public:
    explicit PredictPage(QWidget *parent = nullptr);
    void bindFacade(AdminClientFacade &facade);
    void refresh();
    void applyPredictions(const PredictionBundle &bundle);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void setupHero(QGridLayout *grid);
    void setupChartCard(QGridLayout *grid);
    void setupTableCard(QGridLayout *grid);
    void rebuildFilters();
    void rebuildChart();
    void rebuildTable();
    void startRunPolling();
    void stopRunPolling();
    PredictionList visiblePredictions() const;

    AdminClientFacade *facade_ = nullptr;
    bool loaded_ = false;
    bool running_ = false;
    PredictionBundle bundle_;
    QTimer *pollTimer_ = nullptr;
    QLabel *generatedLabel_ = nullptr;
    QLabel *statusNote_ = nullptr;
    QPushButton *runButton_ = nullptr;
    QComboBox *stationFilter_ = nullptr;
    QComboBox *horizonFilter_ = nullptr;
    QTableWidget *table_ = nullptr;
    QChartView *chartView_ = nullptr;
    QLineSeries *actualSeries_ = nullptr;
    QLineSeries *predictedSeries_ = nullptr;
    QScatterSeries *peakSeries_ = nullptr;
    QCategoryAxis *timeAxis_ = nullptr;
    QValueAxis *energyAxis_ = nullptr;
};

}  // namespace ncs
