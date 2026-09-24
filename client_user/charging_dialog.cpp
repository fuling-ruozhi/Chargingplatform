#include "charging_dialog.h"

#include "confirm_action_dialog.h"
#include "service/user_client_facade.h"
#include "config/charge_config.h"
#include "util/charge_calculator.h"
#include "util/date_time_storage.h"
#include "user_messages.h"
#include "review_dialog.h"

#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace ncs {

namespace {

QString displayTime(const QString &value)
{
    const QDateTime dateTime = DateTimeStorage::fromText(value);
    return dateTime.isValid()
        ? dateTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        : value;
}

QString displayDuration(qint64 seconds)
{
    seconds = qMax<qint64>(0, seconds);
    return QStringLiteral("%1:%2:%3")
        .arg(seconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

QFrame *makeInfoCard(const QString &caption, QLabel **captionLabel,
                     QLabel **valueLabel, QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("metricCard"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(13, 10, 13, 10);
    layout->setSpacing(3);
    *captionLabel = new QLabel(caption, card);
    (*captionLabel)->setObjectName(QStringLiteral("mutedLabel"));
    *valueLabel = new QLabel(card);
    (*valueLabel)->setObjectName(QStringLiteral("cardTitle"));
    layout->addWidget(*captionLabel);
    layout->addWidget(*valueLabel);
    return card;
}

void refreshStyle(QWidget *widget, const QString &objectName)
{
    widget->setObjectName(objectName);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

}

ChargingDialog::ChargingDialog(const ChargingRecord &record, const QString &chargerCode,
                               double stationPrice, UserClientFacade &facade,
                               QWidget *parent, const SmartChargingPlan *smartPlan,
                               const SmartChargingInput *smartInput)
    : QDialog(parent), record_(record), chargerCode_(chargerCode),
      stationPrice_(record.price > 0.0 ? record.price : stationPrice), facade_(facade)
{
    if (smartPlan && smartInput) {
        smartPlan_ = *smartPlan;
        smartInput_ = *smartInput;
        hasSmartPlan_ = true;
    }
    setFixedSize(400, 740);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 14);
    layout->setSpacing(9);
    auto *titleRow = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("充电详情"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    stateLabel_ = new QLabel(this);
    stateLabel_->setAlignment(Qt::AlignCenter);
    titleRow->addWidget(title);
    titleRow->addStretch();
    titleRow->addWidget(stateLabel_);
    layout->addLayout(titleRow);

    auto *orderCard = new QFrame(this);
    orderCard->setObjectName(QStringLiteral("featuredCard"));
    auto *orderLayout = new QVBoxLayout(orderCard);
    orderLayout->setContentsMargins(16, 14, 16, 14);
    orderLayout->setSpacing(4);
    QLabel *stationCaption = nullptr;
    QFrame *stationCard = makeInfoCard(QStringLiteral("当前电站"), &stationCaption,
                                       &stationLabel_, orderCard);
    stationCard->setObjectName(QStringLiteral("metricCard"));
    QLabel *chargerCaption = nullptr;
    QFrame *chargerCard = makeInfoCard(QStringLiteral("当前充电桩"), &chargerCaption,
                                       &chargerLabel_, orderCard);
    chargerCard->setObjectName(QStringLiteral("metricCard"));
    auto *identityRow = new QHBoxLayout;
    identityRow->addWidget(stationCard, 1);
    identityRow->addWidget(chargerCard, 1);
    orderLayout->addLayout(identityRow);
    layout->addWidget(orderCard);

    auto *timeRow = new QHBoxLayout;
    timeRow->setSpacing(8);
    timeRow->addWidget(makeInfoCard(QStringLiteral("预约时间"), &startCaption_,
                                    &startLabel_, this), 1);
    timeRow->addWidget(makeInfoCard(QStringLiteral("预约剩余时间"), &timeCaption_,
                                    &timeLabel_, this), 1);
    timeLabel_->setObjectName(QStringLiteral("primaryNumber"));
    layout->addLayout(timeRow);

    metricsFrame_ = new QFrame(this);
    metricsFrame_->setObjectName(QStringLiteral("metricCard"));
    auto *metricsLayout = new QGridLayout(metricsFrame_);
    metricsLayout->setContentsMargins(14, 12, 14, 12);
    metricsLayout->setHorizontalSpacing(18);
    metricsLayout->setVerticalSpacing(8);
    auto addMetric = [&](int row, int column, const QString &caption,
                         QLabel **value, QLabel **captionResult = nullptr) {
        auto *box = new QVBoxLayout;
        auto *captionLabel = new QLabel(caption, metricsFrame_);
        captionLabel->setObjectName(QStringLiteral("mutedLabel"));
        *value = new QLabel(metricsFrame_);
        (*value)->setObjectName(QStringLiteral("primaryNumber"));
        box->addWidget(captionLabel);
        box->addWidget(*value);
        metricsLayout->addLayout(box, row, column);
        if (captionResult) *captionResult = captionLabel;
    };
    addMetric(0, 0, QStringLiteral("当前电量"), &energyLabel_);
    addMetric(0, 1, QStringLiteral("预计费用"), &costLabel_, &costCaption_);
    addMetric(1, 0, QStringLiteral("实时功率"), &powerLabel_);
    addMetric(1, 1, QStringLiteral("当前电量 SoC"), &socLabel_);
    socProgress_ = new QProgressBar(metricsFrame_);
    socProgress_->setRange(0, 1000);
    socProgress_->setTextVisible(false);
    metricsLayout->addWidget(socProgress_, 2, 0, 1, 2);
    layout->addWidget(metricsFrame_);

    receiptFrame_ = new QFrame(this);
    receiptFrame_->setObjectName(QStringLiteral("receiptCard"));
    auto *receiptLayout = new QVBoxLayout(receiptFrame_);
    receiptLayout->setContentsMargins(13, 9, 13, 9);
    receiptLabel_ = new QLabel(receiptFrame_);
    receiptLabel_->setObjectName(QStringLiteral("receiptDetails"));
    receiptLabel_->setWordWrap(true);
    receiptLayout->addWidget(receiptLabel_);
    receiptFrame_->hide();
    layout->addWidget(receiptFrame_);
    layout->addStretch();

    actionButton_ = new QPushButton(this);
    cancelButton_ = new QPushButton(QStringLiteral("取消预约"), this);
    cancelButton_->setObjectName(QStringLiteral("ghostButton"));
    layout->addWidget(actionButton_);
    layout->addWidget(cancelButton_);

    stationLabel_->setText(record_.stationName.isEmpty()
        ? QStringLiteral("当前电站") : record_.stationName);
    chargerLabel_->setText(record_.chargerCode.isEmpty()
        ? (chargerCode_.isEmpty() ? QStringLiteral("当前电桩") : chargerCode_)
        : record_.chargerCode);

    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &ChargingDialog::refresh);
    timer_->start(1000);

    connect(actionButton_, &QPushButton::clicked, this, [this] {
        if (!pendingRoute_.isEmpty()) return;
        if (record_.status == ChargingOrderStatus::Reserved) {
            setRequestState(QStringLiteral("正在启动充电，请稍候…"));
            pendingRoute_ = QStringLiteral("charge.start");
            facade_.startCharge(record_.userId, record_.chargerId, record_.id);
        } else if (record_.status == ChargingOrderStatus::Charging) {
            if (confirmation_) return;
            auto *dialog = new ConfirmActionDialog(
                QStringLiteral("结束充电"),
                QStringLiteral("确认结束本次充电并按实际电量结算吗？"),
                QStringLiteral("结束并结算"), ConfirmActionDialog::Severity::Danger,
                this);
            confirmation_ = dialog;
            connect(dialog, &QDialog::finished, this, [this, dialog] {
                confirmation_.clear();
                QTimer::singleShot(0, this, [dialog] { delete dialog; });
            });
            connect(dialog, &ConfirmActionDialog::confirmed, this, [this] {
                setRequestState(QStringLiteral("正在结算，请稍候…"));
                pendingRoute_ = QStringLiteral("charge.settle");
                facade_.stopCharge(record_.id);
                QTimer::singleShot(10000, this, [this] {
                    if (pendingRoute_ == QStringLiteral("charge.settle")) {
                        const QString message = QStringLiteral(
                            "请求超时，请确认服务端正常运行后重试");
                        if (confirmation_) confirmation_->finishFailure(message);
                        resetRequestState(message);
                    }
                });
            });
            dialog->open();
            return;
        } else if (record_.status == ChargingOrderStatus::Completed) {
            openReviewDialog();
        } else {
            accept();
            return;
        }
        if (record_.status == ChargingOrderStatus::Completed) return;
        const QString route = pendingRoute_;
        QTimer::singleShot(10000, this, [this, route] {
            if (pendingRoute_ == route) {
                resetRequestState(QStringLiteral("请求超时，请确认服务端正常运行后重试"));
            }
        });
    });

    connect(cancelButton_, &QPushButton::clicked, this, [this] {
        if (record_.status == ChargingOrderStatus::Completed) {
            accept();
            return;
        }
        if (!pendingRoute_.isEmpty() || record_.status != ChargingOrderStatus::Reserved) return;
        if (confirmation_) return;
        auto *dialog = new ConfirmActionDialog(
            QStringLiteral("取消预约"),
            QStringLiteral("确认取消本次预约并释放充电桩吗？"),
            QStringLiteral("确认取消"), ConfirmActionDialog::Severity::Danger,
            this);
        confirmation_ = dialog;
        connect(dialog, &QDialog::finished, this, [this, dialog] {
            confirmation_.clear();
            QTimer::singleShot(0, this, [dialog] { delete dialog; });
        });
        connect(dialog, &ConfirmActionDialog::confirmed, this, [this] {
            setRequestState(QStringLiteral("正在取消预约，请稍候…"));
            pendingRoute_ = QStringLiteral("charge.cancel");
            facade_.cancelReservation(record_.userId, record_.id);
            QTimer::singleShot(10000, this, [this] {
                if (pendingRoute_ == QStringLiteral("charge.cancel")) {
                    const QString message = QStringLiteral(
                        "请求超时，请确认服务端正常运行后重试");
                    if (confirmation_) confirmation_->finishFailure(message);
                    resetRequestState(message);
                }
            });
        });
        dialog->open();
    });

    connect(&facade_, &UserClientFacade::chargeStarted, this,
            [this](const ChargingRecord &started) {
                if (started.id != record_.id) return;
                pendingRoute_.clear();
                record_ = started;
                showCharging();
            });
    connect(&facade_, &UserClientFacade::chargeStopped, this,
            [this](const ChargingRecord &completed) {
                if (completed.id != record_.id) return;
                if (confirmation_) confirmation_->finishSuccess();
                pendingRoute_.clear();
                record_ = completed;
                timer_->stop();
                setWindowTitle(QStringLiteral("充电完成"));
                refreshStyle(stateLabel_, QStringLiteral("statusSuccess"));
                stateLabel_->setText(QStringLiteral("充电完成，记录已保存"));
                timeLabel_->setText(displayDuration(record_.durationSeconds));
                energyLabel_->setText(QStringLiteral("%1 kWh").arg(record_.energy, 0, 'f', 2));
                costCaption_->setText(QStringLiteral("本次费用"));
                costLabel_->setText(QStringLiteral("%1 元").arg(record_.cost, 0, 'f', 2));
                powerLabel_->setText(QStringLiteral("%1 kW").arg(record_.powerKw, 0, 'f', 1));
                socLabel_->setText(QStringLiteral("%1%").arg(record_.finalSoc, 0, 'f', 1));
                socProgress_->setValue(qRound(record_.finalSoc * 10.0));
                receiptLabel_->setText(
                    QStringLiteral("订单号：%1\n结束时间：%2\n电价：%3 元/kWh\n"
                                   "欠费：%4 元\n扣款后余额：%5 元")
                        .arg(record_.orderNo, displayTime(record_.endTime))
                        .arg(record_.price, 0, 'f', 2)
                        .arg(record_.debtAmount, 0, 'f', 2)
                        .arg(record_.balanceAfter, 0, 'f', 2));
                receiptFrame_->show();
                refreshStyle(actionButton_, QString());
                actionButton_->setText(QStringLiteral("立即评价"));
                actionButton_->setEnabled(true);
                cancelButton_->setText(QStringLiteral("稍后评价"));
                cancelButton_->show();
                cancelButton_->setEnabled(true);
            });
    connect(&facade_, &UserClientFacade::reservationCancelled, this,
            [this](const ChargingRecord &cancelled) {
                if (cancelled.id != record_.id) return;
                if (confirmation_) confirmation_->finishSuccess();
                pendingRoute_.clear();
                record_ = cancelled;
                timer_->stop();
                setWindowTitle(QStringLiteral("预约已取消"));
                refreshStyle(stateLabel_, QStringLiteral("statusSuccess"));
                stateLabel_->setText(QStringLiteral("预约已取消，充电桩已释放"));
                timeLabel_->setText(QStringLiteral("已取消"));
                metricsFrame_->hide();
                refreshStyle(actionButton_, QString());
                actionButton_->setText(QStringLiteral("关闭"));
                actionButton_->setEnabled(true);
                cancelButton_->hide();
            });
    connect(&facade_, &UserClientFacade::activeChargeReceived, this,
            [this](bool hasActive, const ChargingRecord &active) {
                if (!expiryCheckRequested_) return;
                expiryCheckRequested_ = false;
                pendingRoute_.clear();
                if (!hasActive) {
                    record_.status = ChargingOrderStatus::Cancelled;
                    timer_->stop();
                    setWindowTitle(QStringLiteral("预约已过期"));
                    refreshStyle(stateLabel_, QStringLiteral("statusFault"));
                    stateLabel_->setText(QStringLiteral("预约已过期，充电桩已自动释放"));
                    timeLabel_->setText(QStringLiteral("已过期"));
                    actionButton_->setText(QStringLiteral("关闭"));
                    actionButton_->setEnabled(true);
                    cancelButton_->hide();
                    return;
                }
                if (active.id != record_.id) return;
                record_ = active;
                if (record_.status == ChargingOrderStatus::Reserved) showReservation();
                else showCharging();
            });
    connect(&facade_, &UserClientFacade::requestFailed, this,
            [this](const QString &route, int, const QString &message) {
                if (route != pendingRoute_) return;
                const QString display = userFacingError(message);
                if (confirmation_) confirmation_->finishFailure(display);
                resetRequestState(display);
            });
    connect(&facade_, &UserClientFacade::networkError, this,
            [this](const QString &message) {
                if (pendingRoute_.isEmpty()) return;
                const QString display = userFacingError(message);
                if (confirmation_) confirmation_->finishFailure(display);
                resetRequestState(display);
            });

    if (record_.status == ChargingOrderStatus::Reserved) showReservation();
    else showCharging();
    refresh();
}

}  // namespace ncs
