#include "order_history_dialog.h"

#include "service/user_client_facade.h"
#include "review_dialog.h"
#include "user_messages.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

namespace ncs {
namespace {

QString statusText(ChargingOrderStatus status)
{
    switch (status) {
    case ChargingOrderStatus::Reserved: return QStringLiteral("已预约");
    case ChargingOrderStatus::Charging: return QStringLiteral("充电中");
    case ChargingOrderStatus::Completed: return QStringLiteral("已完成");
    case ChargingOrderStatus::Cancelled: return QStringLiteral("已取消");
    }
    return QStringLiteral("未知状态");
}

QString statusStyleName(ChargingOrderStatus status)
{
    switch (status) {
    case ChargingOrderStatus::Reserved: return QStringLiteral("statusUsing");
    case ChargingOrderStatus::Charging: return QStringLiteral("statusCharging");
    case ChargingOrderStatus::Completed: return QStringLiteral("statusSuccess");
    case ChargingOrderStatus::Cancelled: return QStringLiteral("statusCancelled");
    }
    return QStringLiteral("statusCancelled");
}

QString displayDuration(qint64 seconds)
{
    seconds = qMax<qint64>(0, seconds);
    return QStringLiteral("%1:%2:%3")
        .arg(seconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

void clearLayout(QLayout *layout)
{
    while (layout->count() > 0) {
        QLayoutItem *item = layout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
}

}

OrderHistoryDialog::OrderHistoryDialog(UserClientFacade &facade, QWidget *parent)
    : QDialog(parent), facade_(facade), timer_(new QTimer(this))
{
    setWindowTitle(QStringLiteral("我的订单"));
    setFixedSize(410, 700);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 14);
    layout->setSpacing(10);
    auto *title = new QLabel(QStringLiteral("我的订单"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    stateLabel_ = new QLabel(QStringLiteral("正在读取订单…"), this);
    stateLabel_->setObjectName(QStringLiteral("statusLoading"));
    stateLabel_->setWordWrap(true);
    layout->addWidget(title);
    layout->addWidget(stateLabel_);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto *container = new QWidget(scroll);
    ordersLayout_ = new QVBoxLayout(container);
    ordersLayout_->setContentsMargins(0, 0, 4, 0);
    ordersLayout_->setSpacing(9);
    scroll->setWidget(container);
    layout->addWidget(scroll, 1);
    auto *closeButton = new QPushButton(QStringLiteral("返回用户中心"), this);
    closeButton->setObjectName(QStringLiteral("ghostButton"));
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(closeButton);

    timer_->setSingleShot(true);
    timer_->setInterval(10000);
    connect(timer_, &QTimer::timeout, this, [this] {
        pendingOrderId_ = 0;
        setButtonsEnabled(true);
        setState(QStringLiteral("请求超时，请稍后重试"), true);
    });
    reviewTimer_ = new QTimer(this);
    reviewTimer_->setSingleShot(true);
    reviewTimer_->setInterval(10000);
    connect(reviewTimer_, &QTimer::timeout, this, [this] {
        if (pendingReviewOrderId_ == 0) return;
        const qint64 orderId = pendingReviewOrderId_;
        pendingReviewOrderId_ = 0;
        if (reviewLabels_.contains(orderId)) {
            reviewLabels_.value(orderId)->setText(QStringLiteral("评价状态读取失败"));
        }
        requestNextReview();
    });
    connect(&facade_, &UserClientFacade::ordersReceived,
            this, [this](const QVector<ChargingRecord> &records) {
                timer_->stop();
                render(records);
            });
    connect(&facade_, &UserClientFacade::orderDetailReceived,
            this, [this](const ChargingRecord &record) {
                if (record.id != pendingOrderId_) return;
                timer_->stop();
                pendingOrderId_ = 0;
                setButtonsEnabled(true);
                setState(QStringLiteral("订单详情已加载"));
                showReceipt(record);
            });
    connect(&facade_, &UserClientFacade::reviewReceived, this,
            [this](bool hasReview, const Review &review) {
                if (pendingReviewOrderId_ == 0) return;
                const qint64 orderId = pendingReviewOrderId_;
                pendingReviewOrderId_ = 0;
                reviewTimer_->stop();
                updateReviewState(orderId, hasReview, review);
                requestNextReview();
            });
    connect(&facade_, &UserClientFacade::requestFailed, this,
            [this](const QString &route, int, const QString &message) {
                if (route != QStringLiteral("order.list")
                    && route != QStringLiteral("order.detail")
                    && route != QStringLiteral("review.get")) return;
                if (route == QStringLiteral("review.get")) {
                    if (pendingReviewOrderId_ != 0) {
                        const qint64 orderId = pendingReviewOrderId_;
                        pendingReviewOrderId_ = 0;
                        reviewTimer_->stop();
                        if (reviewLabels_.contains(orderId)) {
                            reviewLabels_.value(orderId)->setText(QStringLiteral("评价状态读取失败"));
                        }
                        requestNextReview();
                    }
                    return;
                }
                timer_->stop();
                pendingOrderId_ = 0;
                setButtonsEnabled(true);
                setState(userFacingError(message), true);
            });
    connect(&facade_, &UserClientFacade::networkError, this,
            [this](const QString &message) {
                if (pendingReviewOrderId_ != 0) {
                    const qint64 orderId = pendingReviewOrderId_;
                    pendingReviewOrderId_ = 0;
                    reviewTimer_->stop();
                    if (reviewLabels_.contains(orderId)) {
                        reviewLabels_.value(orderId)->setText(userFacingError(message));
                    }
                    requestNextReview();
                    return;
                }
                timer_->stop();
                pendingOrderId_ = 0;
                setButtonsEnabled(true);
                setState(userFacingError(message), true);
            });

    timer_->start();
    facade_.requestOrders(1, 100);
}

void OrderHistoryDialog::render(const QVector<ChargingRecord> &records)
{
    clearLayout(ordersLayout_);
    detailButtons_.clear();
    reviewLabels_.clear();
    reviewButtons_.clear();
    reviewQueue_.clear();
    reviewKnown_.clear();
    reviews_.clear();
    pendingReviewOrderId_ = 0;
    reviewTimer_->stop();
    if (records.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("还没有订单记录"), this);
        empty->setObjectName(QStringLiteral("mutedLabel"));
        empty->setAlignment(Qt::AlignCenter);
        ordersLayout_->addStretch();
        ordersLayout_->addWidget(empty);
        ordersLayout_->addStretch();
        setState(QStringLiteral("暂无订单"));
        return;
    }
    for (const ChargingRecord &record : records) {
        auto *card = new QFrame(this);
        card->setObjectName(QStringLiteral("orderCard"));
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(14, 13, 14, 13);
        cardLayout->setSpacing(9);
        auto *top = new QHBoxLayout;
        auto *station = new QLabel(record.stationName, card);
        station->setObjectName(QStringLiteral("cardTitle"));
        auto *status = new QLabel(statusText(record.status), card);
        status->setObjectName(statusStyleName(record.status));
        top->addWidget(station);
        top->addStretch();
        top->addWidget(status);
        const QString time = record.startTime.isEmpty()
            ? record.reservedAt : record.startTime;
        auto *charger = new QLabel(record.chargerCode, card);
        charger->setObjectName(QStringLiteral("mutedLabel"));
        auto *timeLabel = new QLabel(time, card);
        timeLabel->setObjectName(QStringLiteral("captionLabel"));
        auto *summaryRow = new QHBoxLayout;
        auto *energy = new QLabel(
            QStringLiteral("%1 kWh").arg(record.energy, 0, 'f', 2), card);
        energy->setObjectName(QStringLiteral("mutedLabel"));
        auto *cost = new QLabel(
            QStringLiteral("%1 元").arg(record.cost, 0, 'f', 2), card);
        cost->setObjectName(QStringLiteral("priceLabel"));
        summaryRow->addWidget(energy);
        summaryRow->addStretch();
        summaryRow->addWidget(cost);
        auto *detail = new QPushButton(QStringLiteral("查看小票"), card);
        detail->setObjectName(QStringLiteral("secondaryButton"));
        detail->setProperty("orderId", record.id);
        connect(detail, &QPushButton::clicked, this,
                [this, record] { requestDetail(record.id); });
        detailButtons_.append(detail);
        cardLayout->addLayout(top);
        cardLayout->addWidget(charger);
        cardLayout->addWidget(timeLabel);
        cardLayout->addLayout(summaryRow);
        cardLayout->addWidget(detail);
        if (record.status == ChargingOrderStatus::Completed) {
            auto *reviewRow = new QHBoxLayout;
            auto *reviewLabel = new QLabel(QStringLiteral("评价状态读取中…"), card);
            reviewLabel->setObjectName(QStringLiteral("mutedLabel"));
            auto *reviewButton = new QPushButton(QStringLiteral("评价"), card);
            reviewButton->setObjectName(QStringLiteral("secondaryButton"));
            reviewButton->setEnabled(false);
            connect(reviewButton, &QPushButton::clicked, this,
                    [this, orderId = record.id] { openReview(orderId); });
            reviewLabels_.insert(record.id, reviewLabel);
            reviewButtons_.insert(record.id, reviewButton);
            reviewQueue_.enqueue(record.id);
            reviewRow->addWidget(reviewLabel);
            reviewRow->addStretch();
            reviewRow->addWidget(reviewButton);
            cardLayout->addLayout(reviewRow);
        }
        ordersLayout_->addWidget(card);
    }
    ordersLayout_->addStretch();
    setState(QStringLiteral("共 %1 条订单").arg(records.size()));
    requestNextReview();
}

void OrderHistoryDialog::requestNextReview()
{
    if (pendingReviewOrderId_ != 0 || reviewQueue_.isEmpty()) return;
    pendingReviewOrderId_ = reviewQueue_.dequeue();
    reviewTimer_->start();
    facade_.requestReview(pendingReviewOrderId_);
}

void OrderHistoryDialog::updateReviewState(qint64 orderId, bool hasReview,
                                           const Review &review)
{
    reviewKnown_.insert(orderId);
    if (hasReview) reviews_.insert(orderId, review);
    if (!reviewLabels_.contains(orderId) || !reviewButtons_.contains(orderId)) return;
    if (hasReview) {
        reviewLabels_.value(orderId)->setText(
            QStringLiteral("已评价 %1 / 5.00").arg(review.overallScore, 0, 'f', 2));
        reviewButtons_.value(orderId)->setText(QStringLiteral("已评价"));
        reviewButtons_.value(orderId)->setEnabled(false);
    } else {
        reviewLabels_.value(orderId)->setText(QStringLiteral("待评价"));
        reviewButtons_.value(orderId)->setEnabled(true);
    }
}

void OrderHistoryDialog::openReview(qint64 orderId)
{
    if (!reviewKnown_.contains(orderId) || reviews_.contains(orderId)) return;
    ReviewDialog dialog(orderId, facade_, this);
    connect(&dialog, &ReviewDialog::reviewCompleted, this,
            [this, orderId](const Review &review) {
                updateReviewState(orderId, true, review);
            });
    dialog.exec();
}

void OrderHistoryDialog::requestDetail(qint64 orderId)
{
    if (pendingOrderId_ != 0) return;
    pendingOrderId_ = orderId;
    setButtonsEnabled(false);
    setState(QStringLiteral("正在读取小票…"));
    timer_->start();
    facade_.requestOrderDetail(orderId);
}

void OrderHistoryDialog::showReceipt(const ChargingRecord &record)
{
    QDialog receipt(this);
    receipt.setWindowTitle(QStringLiteral("订单小票"));
    receipt.setFixedSize(370, 590);
    auto *layout = new QVBoxLayout(&receipt);
    layout->setContentsMargins(16, 16, 16, 14);
    layout->setSpacing(10);
    auto *title = new QLabel(QStringLiteral("充电订单小票"), &receipt);
    title->setObjectName(QStringLiteral("pageTitle"));
    auto *hero = new QFrame(&receipt);
    hero->setObjectName(QStringLiteral("featuredCard"));
    auto *heroLayout = new QVBoxLayout(hero);
    heroLayout->setContentsMargins(16, 14, 16, 14);
    auto *orderNo = new QLabel(record.orderNo, hero);
    orderNo->setObjectName(QStringLiteral("heroTitle"));
    auto *station = new QLabel(record.stationName, hero);
    station->setObjectName(QStringLiteral("heroSubtitle"));
    auto *status = new QLabel(statusText(record.status), hero);
    status->setObjectName(statusStyleName(record.status));
    heroLayout->addWidget(orderNo);
    heroLayout->addWidget(station);
    heroLayout->addWidget(status, 0, Qt::AlignLeft);

    auto *detailsCard = new QFrame(&receipt);
    detailsCard->setObjectName(QStringLiteral("receiptCard"));
    auto *detailsLayout = new QVBoxLayout(detailsCard);
    detailsLayout->setContentsMargins(15, 13, 15, 13);
    const QStringList detailLines{
        QStringLiteral("充电桩  %1").arg(record.chargerCode),
        QStringLiteral("开始时间  %1").arg(record.startTime),
        QStringLiteral("结束时间  %1").arg(record.endTime),
        QStringLiteral("充电时长  %1").arg(displayDuration(record.durationSeconds)),
        QStringLiteral("充电电量  %1 kWh").arg(record.energy, 0, 'f', 2),
        QStringLiteral("电价  %1 元/kWh").arg(record.price, 0, 'f', 2)};
    auto *details = new QLabel(detailLines.join(QLatin1Char('\n')), detailsCard);
    details->setObjectName(QStringLiteral("receiptDetails"));
    details->setWordWrap(true);
    detailsLayout->addWidget(details);

    auto *amountCard = new QFrame(&receipt);
    amountCard->setObjectName(QStringLiteral("metricCard"));
    auto *amountLayout = new QVBoxLayout(amountCard);
    amountLayout->setContentsMargins(15, 12, 15, 12);
    auto *amountCaption = new QLabel(QStringLiteral("本次结算"), amountCard);
    amountCaption->setObjectName(QStringLiteral("mutedLabel"));
    auto *amount = new QLabel(
        QStringLiteral("%1 元").arg(record.cost, 0, 'f', 2), amountCard);
    amount->setObjectName(QStringLiteral("primaryNumber"));
    auto *balance = new QLabel(
        QStringLiteral("欠费 %1 元  ·  扣款后余额 %2 元")
            .arg(record.debtAmount, 0, 'f', 2)
            .arg(record.balanceAfter, 0, 'f', 2), amountCard);
    balance->setObjectName(QStringLiteral("mutedLabel"));
    amountLayout->addWidget(amountCaption);
    amountLayout->addWidget(amount);
    amountLayout->addWidget(balance);
    auto *close = new QPushButton(QStringLiteral("关闭"), &receipt);
    connect(close, &QPushButton::clicked, &receipt, &QDialog::accept);
    layout->addWidget(title);
    layout->addWidget(hero);
    layout->addWidget(detailsCard, 1);
    layout->addWidget(amountCard);
    layout->addWidget(close);
    receipt.exec();
}

void OrderHistoryDialog::setState(const QString &message, bool error)
{
    stateLabel_->setObjectName(error ? QStringLiteral("statusError")
                                     : QStringLiteral("statusSuccess"));
    stateLabel_->style()->unpolish(stateLabel_);
    stateLabel_->style()->polish(stateLabel_);
    stateLabel_->setText(message);
}

void OrderHistoryDialog::setButtonsEnabled(bool enabled)
{
    for (QPushButton *button : detailButtons_) button->setEnabled(enabled);
}

}
