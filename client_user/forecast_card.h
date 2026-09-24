#pragma once

#include <QFrame>

class QLabel;
class QProgressBar;

namespace ncs {

class UserClientFacade;
struct ChargeForecast;

// EXT-SC-03 续航预测卡片：主页（充电入口）与个人中心复用
class ForecastCard : public QFrame
{
    Q_OBJECT
public:
    explicit ForecastCard(UserClientFacade &facade, QWidget *parent = nullptr);
    void refresh();

private:
    void applyForecast(const ChargeForecast &forecast);

    UserClientFacade &facade_;
    QProgressBar *socBar_ = nullptr;
    QLabel *socLabel_ = nullptr;
    QLabel *summaryLabel_ = nullptr;
    QLabel *noteLabel_ = nullptr;
};

}
