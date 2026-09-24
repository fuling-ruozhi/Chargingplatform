#include "reminder_banner.h"

#include "model/charger.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

namespace ncs {

ReminderBanner::ReminderBanner(QWidget *parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("reminderBanner"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 8, 8);
    messageLabel_ = new QLabel(this);
    messageLabel_->setWordWrap(true);
    closeButton_ = new QPushButton(QStringLiteral("关闭"), this);
    closeButton_->setObjectName(QStringLiteral("ghostButton"));
    closeButton_->setFixedWidth(58);
    layout->addWidget(messageLabel_, 1);
    layout->addWidget(closeButton_);
    connect(closeButton_, &QPushButton::clicked, this, &QWidget::hide);
    hide();
}

void ReminderBanner::showMatches(const QVector<ReminderMatch> &matches)
{
    if (matches.isEmpty()) return;
    if (matches.size() == 1) {
        const auto &match = matches.first();
        QString types;
        for (int type : match.matchedTypes) {
            if (!types.isEmpty()) types += QStringLiteral("、");
            types += chargerTypeText(type);
        }
        if (types.isEmpty()) types = QStringLiteral("符合偏好的桩");
        messageLabel_->setText(QStringLiteral("%1 当前有 %2 个符合条件的空闲桩（%3）")
                                   .arg(match.stationName)
                                   .arg(match.idleMatchedChargers)
                                   .arg(types));
    } else {
        messageLabel_->setText(QStringLiteral("%1 个关注/附近站点满足提醒条件")
                                   .arg(matches.size()));
    }
    show();
    raise();
    QTimer::singleShot(7000, this, &QWidget::hide);
}

}  // namespace ncs
