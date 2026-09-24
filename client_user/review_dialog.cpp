#include "review_dialog.h"

#include "model/business_error.h"
#include "service/user_client_facade.h"
#include "user_messages.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

namespace ncs {
namespace {

const QStringList dimensions{
    QStringLiteral("环境"), QStringLiteral("排队体验"),
    QStringLiteral("设备情况"), QStringLiteral("停车便利")};

void polish(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

}

ReviewDialog::ReviewDialog(qint64 orderId, UserClientFacade &facade, QWidget *parent)
    : QDialog(parent), orderId_(orderId), facade_(facade), scores_(4, 0),
      timeoutTimer_(new QTimer(this))
{
    setWindowTitle(QStringLiteral("充电体验评价"));
    setFixedSize(390, 440);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 16);
    layout->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("充电体验评价"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    auto *hint = new QLabel(QStringLiteral("感谢你的反馈，帮助我们持续改进充电体验"), this);
    hint->setObjectName(QStringLiteral("mutedLabel"));
    hint->setWordWrap(true);
    layout->addWidget(title);
    layout->addWidget(hint);

    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("infoCard"));
    auto *scoresLayout = new QGridLayout(card);
    scoresLayout->setContentsMargins(13, 10, 13, 10);
    scoresLayout->setHorizontalSpacing(4);
    scoresLayout->setVerticalSpacing(8);
    for (int dimension = 0; dimension < dimensions.size(); ++dimension) {
        auto *label = new QLabel(dimensions.at(dimension), card);
        label->setObjectName(QStringLiteral("sectionTitle"));
        scoresLayout->addWidget(label, dimension, 0);
        QVector<QPushButton *> buttons;
        for (int score = 1; score <= 5; ++score) {
            auto *button = new QPushButton(QStringLiteral("★"), card);
            button->setObjectName(QStringLiteral("reviewStarButton"));
            button->setProperty("selected", false);
            button->setFixedSize(34, 34);
            connect(button, &QPushButton::clicked, this,
                    [this, dimension, score] { selectScore(dimension, score); });
            buttons.append(button);
            scoresLayout->addWidget(button, dimension, score);
        }
        starButtons_.append(buttons);
    }
    layout->addWidget(card);

    previewLabel_ = new QLabel(QStringLiteral("综合评分：-- / 5.00"), this);
    previewLabel_->setObjectName(QStringLiteral("reviewPreviewLabel"));
    previewLabel_->setAlignment(Qt::AlignCenter);
    layout->addWidget(previewLabel_);
    stateLabel_ = new QLabel(QStringLiteral("请选择四项评分"), this);
    stateLabel_->setObjectName(QStringLiteral("mutedLabel"));
    stateLabel_->setAlignment(Qt::AlignCenter);
    layout->addWidget(stateLabel_);
    layout->addStretch();

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    laterButton_ = new QPushButton(QStringLiteral("稍后评价"), this);
    laterButton_->setObjectName(QStringLiteral("ghostButton"));
    submitButton_ = new QPushButton(QStringLiteral("提交评价"), this);
    submitButton_->setObjectName(QStringLiteral("reviewSubmitButton"));
    submitButton_->setEnabled(false);
    buttons->addWidget(laterButton_);
    buttons->addWidget(submitButton_);
    layout->addLayout(buttons);

    timeoutTimer_->setSingleShot(true);
    timeoutTimer_->setInterval(10000);
    connect(timeoutTimer_, &QTimer::timeout, this, [this] {
        if (submitting_) resetAfterFailure(QStringLiteral("请求超时，请稍后重试"));
    });
    connect(laterButton_, &QPushButton::clicked, this, &QDialog::reject);
    connect(submitButton_, &QPushButton::clicked, this, [this] {
        if (submitting_ || scores_.contains(0)) return;
        setSubmitting(true);
        facade_.submitReview(orderId_, scores_.at(0), scores_.at(1),
                             scores_.at(2), scores_.at(3));
        timeoutTimer_->start();
    });
    connect(&facade_, &UserClientFacade::reviewSubmitted, this,
            [this](const Review &review) {
                if (!submitting_ || review.orderId != orderId_) return;
                timeoutTimer_->stop();
                QMessageBox::information(this, QStringLiteral("评价成功"),
                                         QStringLiteral("感谢评价，综合评分：%1 / 5.00")
                                             .arg(review.overallScore, 0, 'f', 2));
                emit reviewCompleted(review);
                accept();
            });
    connect(&facade_, &UserClientFacade::requestFailed, this,
            [this](const QString &route, int code, const QString &message) {
                if (route != QStringLiteral("review.submit") || !submitting_) return;
                timeoutTimer_->stop();
                resetAfterFailure(friendlyError(code, message));
            });
    connect(&facade_, &UserClientFacade::networkError, this,
            [this](const QString &message) {
                if (!submitting_) return;
                timeoutTimer_->stop();
                resetAfterFailure(userFacingError(message));
            });
}

void ReviewDialog::selectScore(int dimension, int score)
{
    if (submitting_) return;
    scores_[dimension] = score;
    for (int index = 0; index < starButtons_.at(dimension).size(); ++index) {
        auto *button = starButtons_.at(dimension).at(index);
        button->setProperty("selected", index < score);
        polish(button);
    }
    refreshPreview();
}

void ReviewDialog::refreshPreview()
{
    if (scores_.contains(0)) {
        previewLabel_->setText(QStringLiteral("综合评分：-- / 5.00"));
        stateLabel_->setText(QStringLiteral("请选择四项评分"));
        submitButton_->setEnabled(false);
        return;
    }
    double total = 0.0;
    for (const int score : scores_) total += score;
    previewLabel_->setText(QStringLiteral("综合评分：%1 / 5.00")
                               .arg(total / scores_.size(), 0, 'f', 2));
    stateLabel_->setText(QStringLiteral("预览分数仅供参考，最终评分以服务端为准"));
    submitButton_->setEnabled(true);
}

void ReviewDialog::setSubmitting(bool submitting)
{
    submitting_ = submitting;
    submitButton_->setEnabled(!submitting && !scores_.contains(0));
    laterButton_->setEnabled(!submitting);
    for (const auto &buttons : starButtons_) {
        for (QPushButton *button : buttons) button->setEnabled(!submitting);
    }
    stateLabel_->setText(submitting ? QStringLiteral("正在提交评价…")
                                    : QStringLiteral("请选择四项评分"));
}

void ReviewDialog::resetAfterFailure(const QString &message)
{
    setSubmitting(false);
    stateLabel_->setObjectName(QStringLiteral("statusError"));
    stateLabel_->setText(message);
    polish(stateLabel_);
}

QString ReviewDialog::friendlyError(int code, const QString &message) const
{
    switch (static_cast<BusinessErrorCode>(code)) {
    case BusinessErrorCode::AuthRequired:
    case BusinessErrorCode::SessionExpired:
    case BusinessErrorCode::Forbidden:
        return QStringLiteral("登录状态已失效，请重新登录");
    case BusinessErrorCode::OrderNotFound:
        return QStringLiteral("订单不存在，无法评价");
    case BusinessErrorCode::InvalidOrderOwner:
        return QStringLiteral("无法评价该订单");
    case BusinessErrorCode::InvalidOrderState:
        return QStringLiteral("订单尚未结算，暂时不能评价");
    case BusinessErrorCode::ReviewAlreadyExists:
        return QStringLiteral("该订单已经评价过了");
    case BusinessErrorCode::InvalidReviewScore:
        return QStringLiteral("评分必须在 1 到 5 之间");
    case BusinessErrorCode::DatabaseError:
        return QStringLiteral("服务暂时不可用，请稍后重试");
    default:
        return userFacingError(message);
    }
}

}
