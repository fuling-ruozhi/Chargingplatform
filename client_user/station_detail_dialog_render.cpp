#include "station_detail_dialog.h"

#include "model/charger.h"
#include "service/user_client_facade.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include <functional>

namespace ncs {

namespace {

void refreshStyle(QWidget *widget, const QString &objectName)
{
    widget->setObjectName(objectName);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

void clearLayout(QLayout *layout)
{
    while (layout->count() > 0) {
        QLayoutItem *item = layout->takeAt(0);
        if (item->layout()) clearLayout(item->layout());
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
}

}  // namespace

void StationDetailDialog::renderChargers()
{
    clearLayout(chargerLayout_);
    actionButtons_.clear();
    int idleCount = 0;
    for (const Charger &charger : detail_.chargers) {
        if (charger.status == ChargerStatus::Idle) ++idleCount;
    }
    refreshStyle(requestStateLabel_, QStringLiteral("statusSuccess"));
    requestStateLabel_->setText(QStringLiteral("共 %1 个充电桩，可用 %2 个")
                                    .arg(detail_.chargers.size()).arg(idleCount));

    for (const Charger &charger : detail_.chargers) {
        const bool mine = hasActive_ && activeRecord_.chargerId == charger.id;
        const bool reserved = charger.activeOrderStatus
            == static_cast<int>(ChargingOrderStatus::Reserved);
        const bool charging = charger.activeOrderStatus
            == static_cast<int>(ChargingOrderStatus::Charging);
        auto *card = new QFrame(this);
        card->setObjectName(QStringLiteral("chargerCard"));
        auto *cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(13, 12, 13, 12);
        cardLayout->setSpacing(10);
        auto *copy = new QVBoxLayout;
        auto *code = new QLabel(charger.code, card);
        code->setObjectName(QStringLiteral("sectionTitle"));
        auto *status = new QLabel(card);
        status->setAlignment(Qt::AlignCenter);
        copy->addWidget(code);
        auto *spec = new QLabel(QStringLiteral("%1 · %2 kW · 已服务 %3 次")
                                    .arg(chargerTypeText(charger.type))
                                    .arg(charger.powerKw, 0, 'f', 1)
                                    .arg(charger.totalCount), card);
        spec->setObjectName(QStringLiteral("mutedLabel"));
        copy->addWidget(spec);
        copy->addWidget(status, 0, Qt::AlignLeft);
        cardLayout->addLayout(copy, 1);

        auto *actions = new QHBoxLayout;
        actions->setSpacing(6);
        auto addAction = [&](const QString &text, const QString &styleName,
                             bool enabled, const std::function<void()> &callback) {
            auto *button = new QPushButton(text, card);
            button->setFixedWidth(72);
            if (!styleName.isEmpty()) button->setObjectName(styleName);
            button->setEnabled(enabled);
            if (callback) connect(button, &QPushButton::clicked, this, callback);
            actionButtons_.append(button);
            actions->addWidget(button);
        };

        if (charger.status == ChargerStatus::Fault) {
            refreshStyle(status, QStringLiteral("statusFault"));
            status->setText(QStringLiteral("故障维护"));
            addAction(QStringLiteral("维护中"), QString(), false, {});
        } else if (mine && activeRecord_.status == ChargingOrderStatus::Reserved) {
            refreshStyle(status, QStringLiteral("statusUsing"));
            status->setText(QStringLiteral("我的预约"));
            addAction(QStringLiteral("开始充电"), QString(), true,
                      [this, charger] {
                          beginAction(QStringLiteral("charge.start"), charger.id,
                                      QStringLiteral("正在启动充电，请稍候…"));
                          facade_.startCharge(userId_, charger.id, activeRecord_.id);
                      });
            addAction(QStringLiteral("取消预约"), QStringLiteral("ghostButton"), true,
                      [this, charger] {
                          beginAction(QStringLiteral("charge.cancel"), charger.id,
                                      QStringLiteral("正在取消预约，请稍候…"));
                          facade_.cancelReservation(userId_, activeRecord_.id);
                      });
        } else if (mine && activeRecord_.status == ChargingOrderStatus::Charging) {
            refreshStyle(status, QStringLiteral("statusIdle"));
            status->setText(QStringLiteral("正在充电"));
            addAction(QStringLiteral("查看充电"), QString(), true,
                      [this] { openCharging(activeRecord_); });
        } else if (reserved) {
            refreshStyle(status, QStringLiteral("statusUsing"));
            status->setText(QStringLiteral("已被预约"));
            addAction(QStringLiteral("已预约"), QString(), false, {});
        } else if (charging || charger.status == ChargerStatus::Using) {
            refreshStyle(status, QStringLiteral("statusUsing"));
            status->setText(QStringLiteral("正在充电"));
            addAction(QStringLiteral("正在使用"), QString(), false, {});
        } else {
            refreshStyle(status, QStringLiteral("statusIdle"));
            status->setText(QStringLiteral("空闲，可使用"));
            addAction(QStringLiteral("预约"), QString(), true,
                      [this, charger] {
                          beginAction(QStringLiteral("charge.reserve"), charger.id,
                                      QStringLiteral("正在预约，请稍候…"));
                          facade_.reserveCharge(userId_, charger.id);
                      });
        }
        cardLayout->addLayout(actions);
        chargerLayout_->addWidget(card);
    }
    chargerLayout_->addStretch();
}

}  // namespace ncs
