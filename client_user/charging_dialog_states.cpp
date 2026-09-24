#include "charging_dialog.h"

#include "review_dialog.h"
#include "service/user_client_facade.h"
#include "config/charge_config.h"
#include "util/charge_calculator.h"
#include "util/date_time_storage.h"

#include <QDateTime>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QStyle>

namespace ncs {
namespace {

void refreshStateStyle(QWidget *widget, const QString &objectName)
{
    widget->setObjectName(objectName);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

QString stateDisplayTime(const QString &value)
{
    const QDateTime dateTime = DateTimeStorage::fromText(value);
    return dateTime.isValid()
        ? dateTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : value;
}

QString stateDisplayDuration(qint64 seconds)
{
    seconds = qMax<qint64>(0, seconds);
    return QStringLiteral("%1:%2:%3")
        .arg(seconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

}

void ChargingDialog::showReservation()
{
    setWindowTitle(QStringLiteral("充电预约"));
    refreshStateStyle(stateLabel_, QStringLiteral("statusUsing"));
    stateLabel_->setText(QStringLiteral("已预约，请在保留时间内开始充电"));
    if (hasSmartPlan_) stateLabel_->setText(
        QStringLiteral("已预约 · 智慧目标 %1%").arg(
            smartPlan_.recommendedTargetSoc, 0, 'f', 1));
    startCaption_->setText(QStringLiteral("预约时间"));
    startLabel_->setText(stateDisplayTime(record_.reservedAt));
    timeCaption_->setText(QStringLiteral("预约剩余时间"));
    metricsFrame_->hide();
    receiptFrame_->hide();
    refreshStateStyle(actionButton_, QString());
    actionButton_->setText(QStringLiteral("开始充电"));
    actionButton_->setEnabled(pendingRoute_.isEmpty());
    cancelButton_->show();
    cancelButton_->setEnabled(pendingRoute_.isEmpty());
}

void ChargingDialog::showCharging()
{
    setWindowTitle(QStringLiteral("充电进行中"));
    refreshStateStyle(stateLabel_, QStringLiteral("statusCharging"));
    stateLabel_->setText(QStringLiteral("正在充电"));
    if (hasSmartPlan_) stateLabel_->setText(
        QStringLiteral("正在充电 · 智慧目标 %1%").arg(
            smartPlan_.recommendedTargetSoc, 0, 'f', 1));
    startCaption_->setText(QStringLiteral("开始时间"));
    startLabel_->setText(stateDisplayTime(record_.startTime));
    timeCaption_->setText(QStringLiteral("已充时间"));
    metricsFrame_->show();
    receiptFrame_->hide();
    costCaption_->setText(QStringLiteral("预计费用"));
    refreshStateStyle(actionButton_, QStringLiteral("dangerButton"));
    actionButton_->setText(QStringLiteral("结束充电并结算"));
    actionButton_->setEnabled(pendingRoute_.isEmpty());
    cancelButton_->hide();
}

void ChargingDialog::openReviewDialog()
{
    ReviewDialog dialog(record_.id, facade_, this);
    if (dialog.exec() != QDialog::Accepted) return;
    actionButton_->setText(QStringLiteral("完成"));
    cancelButton_->hide();
}

void ChargingDialog::setRequestState(const QString &message)
{
    refreshStateStyle(stateLabel_, QStringLiteral("statusLoading"));
    stateLabel_->setText(message);
    actionButton_->setEnabled(false);
    cancelButton_->setEnabled(false);
}

void ChargingDialog::resetRequestState(const QString &message)
{
    pendingRoute_.clear();
    expiryCheckRequested_ = false;
    refreshStateStyle(stateLabel_, QStringLiteral("statusFault"));
    stateLabel_->setText(message);
    actionButton_->setEnabled(true);
    cancelButton_->setEnabled(record_.status == ChargingOrderStatus::Reserved);
}

void ChargingDialog::refresh()
{
    if (record_.status == ChargingOrderStatus::Reserved) {
        const QDateTime expiry = DateTimeStorage::fromText(record_.expireAt);
        const qint64 seconds = DateTimeStorage::now().secsTo(expiry);
        timeLabel_->setText(stateDisplayDuration(seconds));
        if (seconds <= 0 && !expiryCheckRequested_ && pendingRoute_.isEmpty()) {
            expiryCheckRequested_ = true;
            pendingRoute_ = QStringLiteral("charge.active");
            setRequestState(QStringLiteral("预约已到期，正在确认状态…"));
            facade_.requestActiveCharge(record_.userId);
        }
        return;
    }
    if (record_.status != ChargingOrderStatus::Charging) return;
    const ChargeMetrics metrics = ChargeCalculator::calculate(
        DateTimeStorage::fromText(record_.startTime), DateTimeStorage::now(),
        record_.powerKw, stationPrice_, record_.timeScale, record_.initialSoc,
        ChargeConfig::batteryCapacityKwh());
    timeLabel_->setText(stateDisplayDuration(metrics.elapsedSimSeconds));
    energyLabel_->setText(QStringLiteral("%1 kWh").arg(metrics.energyKwh, 0, 'f', 3));
    costLabel_->setText(QStringLiteral("%1 元").arg(metrics.amount, 0, 'f', 2));
    powerLabel_->setText(QStringLiteral("%1 kW").arg(metrics.powerKw, 0, 'f', 1));
    updateSmartSoc(metrics);
}

}
