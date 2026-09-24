#include "station_detail_dialog.h"

#include "charging_dialog.h"
#include "confirm_action_dialog.h"
#include "navigation_launcher.h"
#include "service/user_client_facade.h"
#include "util/navigation_url.h"

#ifdef NCS_HAS_WEBENGINE
#include "map_route_dialog.h"
#include "map_view_dialog.h"
#endif

#include <QLabel>
#include <QFrame>
#include <QHBoxLayout>
#include <QLayoutItem>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <functional>

namespace ncs {
namespace {

void refreshActionStyle(QWidget *widget, const QString &objectName)
{
    widget->setObjectName(objectName);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

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

}

void StationDetailDialog::beginAction(const QString &route, qint64 chargerId,
                                      const QString &message)
{
    if (!pendingRoute_.isEmpty() || awaitingDetail_ || awaitingActive_) return;
    pendingRoute_ = route;
    rechargeButton_->hide();
    pendingChargerId_ = chargerId;
    refreshActionStyle(requestStateLabel_, QStringLiteral("statusLoading"));
    requestStateLabel_->setText(message);
    for (QPushButton *button : actionButtons_) button->setEnabled(false);
    QTimer::singleShot(10000, this, [this, route, chargerId] {
        if (pendingRoute_ == route && pendingChargerId_ == chargerId) {
            resetAction(QStringLiteral("请求超时，请确认服务端正常运行后重试"));
        }
    });
}

void StationDetailDialog::resetAction(const QString &message)
{
    if (pendingRoute_ == QStringLiteral("charge.reserve")) {
        hasPendingSmartPlan_ = false;
    }
    pendingRoute_.clear();
    pendingChargerId_ = 0;
    if (!actionButtons_.isEmpty()) renderChargers();
    refreshActionStyle(requestStateLabel_, QStringLiteral("statusError"));
    requestStateLabel_->setText(message);
}

void StationDetailDialog::openCharging(const ChargingRecord &record)
{
    ChargingDialog charging(record, record.chargerCode, detail_.station.price,
                            facade_, this,
                            hasPendingSmartPlan_ ? &pendingSmartPlan_ : nullptr,
                            hasPendingSmartPlan_ ? &pendingSmartInput_ : nullptr);
    charging.exec();
    hasPendingSmartPlan_ = false;
    requestCurrentState(true);
}

void StationDetailDialog::openNavigation()
{
#ifdef NCS_HAS_WEBENGINE
    // UC-U-04 增强：已定位时在 App 内嵌腾讯地图绘制驾车路线（不跳浏览器）；
    // 无 WebEngine 构建或未定位时保持原浏览器路线兜底。
    if (hasOrigin_) {
        MapRouteDialog dialog(
            originLatitude_, originLongitude_, detail_.station.latitude,
            detail_.station.longitude, detail_.station.name,
            detail_.station.address, this);
        dialog.exec();
        return;
    }
#endif
    const QUrl url = NavigationUrl::build(
        originLongitude_, originLatitude_, detail_.station.longitude,
        detail_.station.latitude, detail_.station.name);
    if (!openExternalUrl(url, winId())) {
        resetAction(QStringLiteral("无法打开系统浏览器，请根据页面坐标手动导航"));
    }
}

#ifdef NCS_HAS_WEBENGINE
void StationDetailDialog::openMapView()
{
    MapViewDialog dialog(detail_.station.longitude, detail_.station.latitude,
                         detail_.station.name, detail_.station.address,
                         detail_.station.distanceKm, this, hasOrigin_,
                         originLongitude_, originLatitude_);
    dialog.exec();
}
#endif

void StationDetailDialog::renderChargers()
{
    clearLayout(chargerLayout_);
    actionButtons_.clear();
    int idleCount = 0;
    for (const Charger &charger : detail_.chargers)
        if (charger.status == ChargerStatus::Idle) ++idleCount;
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
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(14, 12, 14, 12);
        cardLayout->setSpacing(7);
        auto *top = new QHBoxLayout;
        auto *code = new QLabel(charger.code, card);
        code->setObjectName(QStringLiteral("cardTitle"));
        auto *status = new QLabel(card);
        status->setAlignment(Qt::AlignCenter);
        top->addWidget(code); top->addStretch(); top->addWidget(status);
        cardLayout->addLayout(top);
        auto *spec = new QLabel(QStringLiteral("%1  ·  %2 kW  ·  累计服务 %3 次")
            .arg(charger.type == 1 ? QStringLiteral("直流快充") : QStringLiteral("交流慢充"))
            .arg(charger.powerKw, 0, 'f', 1).arg(charger.totalCount), card);
        spec->setObjectName(QStringLiteral("mutedLabel"));
        cardLayout->addWidget(spec);
        auto *actions = new QHBoxLayout;
        actions->setSpacing(6); actions->addStretch();
        auto addAction = [&](const QString &text, const QString &styleName,
                             bool enabled, const std::function<void()> &callback) {
            auto *button = new QPushButton(text, card);
            button->setMinimumWidth(78);
            if (!styleName.isEmpty()) button->setObjectName(styleName);
            button->setEnabled(enabled);
            if (callback) connect(button, &QPushButton::clicked, this, callback);
            actionButtons_.append(button); actions->addWidget(button);
        };
        if (charger.status == ChargerStatus::Fault) {
            refreshStyle(status, QStringLiteral("statusFault"));
            status->setText(QStringLiteral("故障维护"));
            addAction(QStringLiteral("维护中"), QString(), false, {});
        } else if (mine && activeRecord_.status == ChargingOrderStatus::Reserved) {
            refreshStyle(status, QStringLiteral("statusUsing"));
            status->setText(QStringLiteral("我的预约"));
            addAction(QStringLiteral("开始充电"), QString(), true, [this, charger] {
                beginAction(QStringLiteral("charge.start"), charger.id,
                            QStringLiteral("正在启动充电，请稍候…"));
                facade_.startCharge(userId_, charger.id, activeRecord_.id);
            });
            addAction(QStringLiteral("取消预约"), QStringLiteral("ghostButton"), true,
                      [this, charger] {
                if (confirmation_) return;
                auto *dialog = new ConfirmActionDialog(
                    QStringLiteral("取消预约"), QStringLiteral("确认取消本次预约并释放充电桩吗？"),
                    QStringLiteral("确认取消"), ConfirmActionDialog::Severity::Danger, this);
                confirmation_ = dialog;
                connect(dialog, &QDialog::finished, this, [this, dialog] {
                    confirmation_.clear(); QTimer::singleShot(0, this, [dialog] { delete dialog; });
                });
                connect(dialog, &ConfirmActionDialog::confirmed, this, [this, charger] {
                    beginAction(QStringLiteral("charge.cancel"), charger.id,
                                QStringLiteral("正在取消预约，请稍候…"));
                    facade_.cancelReservation(userId_, activeRecord_.id);
                });
                dialog->open();
            });
        } else if (mine && activeRecord_.status == ChargingOrderStatus::Charging) {
            refreshStyle(status, QStringLiteral("statusIdle"));
            status->setText(QStringLiteral("正在充电"));
            addAction(QStringLiteral("查看充电"), QString(), true, [this] { openCharging(activeRecord_); });
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
            addAction(QStringLiteral("智慧方案"), QStringLiteral("secondaryButton"), true,
                      [this, charger] { openSmartCharging(charger); });
            addAction(QStringLiteral("预约"), QString(), true, [this, charger] {
                hasPendingSmartPlan_ = false;
                beginAction(QStringLiteral("charge.reserve"), charger.id,
                            QStringLiteral("正在预约，请稍候…"));
                facade_.reserveCharge(userId_, charger.id);
            });
        }
        cardLayout->addLayout(actions); chargerLayout_->addWidget(card);
    }
    chargerLayout_->addStretch();
}

}  // namespace ncs
