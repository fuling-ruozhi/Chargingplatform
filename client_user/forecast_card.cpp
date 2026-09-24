#include "forecast_card.h"

#include "model/charge_forecast.h"
#include "service/user_client_facade.h"

#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>

namespace ncs {

ForecastCard::ForecastCard(UserClientFacade &facade, QWidget *parent)
    : QFrame(parent), facade_(facade)
{
    setObjectName(QStringLiteral("infoCard"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(6);

    auto *title = new QLabel(QStringLiteral("续航预测"), this);
    title->setObjectName(QStringLiteral("cardTitle"));
    layout->addWidget(title);

    socBar_ = new QProgressBar(this);
    socBar_->setObjectName(QStringLiteral("forecastSocBar"));
    socBar_->setRange(0, 100);
    socBar_->setValue(0);
    socBar_->setTextVisible(false);
    layout->addWidget(socBar_);

    socLabel_ = new QLabel(QStringLiteral("当前电量：--"), this);
    socLabel_->setObjectName(QStringLiteral("forecastSocLabel"));
    layout->addWidget(socLabel_);

    summaryLabel_ = new QLabel(QStringLiteral("正在获取预测…"), this);
    summaryLabel_->setObjectName(QStringLiteral("mutedLabel"));
    summaryLabel_->setWordWrap(true);
    layout->addWidget(summaryLabel_);

    noteLabel_ = new QLabel(this);
    noteLabel_->setObjectName(QStringLiteral("captionLabel"));
    noteLabel_->setWordWrap(true);
    noteLabel_->hide();
    layout->addWidget(noteLabel_);

    connect(&facade_, &UserClientFacade::chargeForecastReceived,
            this, &ForecastCard::applyForecast);
    connect(&facade_, &UserClientFacade::requestFailed, this,
            [this](const QString &route, int, const QString &message) {
                if (route == QStringLiteral("user.forecast")) {
                    summaryLabel_->setText(
                        QStringLiteral("预测暂不可用：%1").arg(message));
                }
            });
    refresh();
}

void ForecastCard::refresh()
{
    facade_.requestChargeForecast();
}

void ForecastCard::applyForecast(const ChargeForecast &forecast)
{
    if (forecast.charging) {
        socBar_->setValue(0);
        socLabel_->setText(QStringLiteral("充电中"));
        summaryLabel_->setText(forecast.note);
        noteLabel_->hide();
        return;
    }
    socBar_->setValue(static_cast<int>(forecast.currentSoc + 0.5));
    socLabel_->setText(QStringLiteral("估计当前电量 SoC ≈ %1%")
                           .arg(forecast.currentSoc, 0, 'f', 0));
    summaryLabel_->setText(QStringLiteral("剩余可用约 %1 天，建议 %2 前充电（%3）")
                               .arg(forecast.remainingDays, 0, 'f', 1)
                               .arg(forecast.suggestedDate, forecast.method));
    noteLabel_->setText(forecast.note);
    noteLabel_->setVisible(forecast.coldStart);
}

}
