#include "user_main_window.h"

#include "service/user_client_facade.h"
#include "station_detail_dialog.h"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTimer>

namespace ncs {

void UserMainWindow::requestStationOpen(qint64 stationId)
{
    if (pendingOpenStationId_ != 0) return;
    pendingOpenStationId_ = stationId;
    if (stationButtons_.contains(stationId)) {
        stationButtons_.value(stationId)->setEnabled(false);
        stationButtons_.value(stationId)->setText(QStringLiteral("加载中…"));
    }
    setState(QStringLiteral("正在加载充电桩…"));
    facade_.requestStationDetail(stationId, originLongitude(), originLatitude());
    QTimer::singleShot(10000, this, [this, stationId] {
        if (pendingOpenStationId_ != stationId) return;
        pendingOpenStationId_ = 0;
        if (stationButtons_.contains(stationId)) {
            stationButtons_.value(stationId)->setEnabled(true);
            stationButtons_.value(stationId)->setText(QStringLiteral("查看电桩"));
        }
        setState(QStringLiteral("请求超时，请确认服务端正常运行后重试"), true);
    });
}

void UserMainWindow::showStationDetail(const StationDetail &detail)
{
    int idleCount = 0;
    for (const Charger &charger : detail.chargers) {
        if (charger.status == ChargerStatus::Idle) ++idleCount;
    }
    if (availabilityLabels_.contains(detail.station.id)) {
        availabilityLabels_.value(detail.station.id)
            ->setText(QStringLiteral("空闲 %1/%2")
                          .arg(idleCount).arg(detail.chargers.size()));
    }
    availabilityPending_.remove(detail.station.id);
    if (availabilityPending_.isEmpty() && pendingOpenStationId_ == 0) {
        setState(QStringLiteral("共 %1 个电站，可用情况已更新").arg(stations_.size()));
    }
    if (detail.station.id != pendingOpenStationId_) return;

    const qint64 stationId = pendingOpenStationId_;
    pendingOpenStationId_ = 0;
    if (stationButtons_.contains(stationId)) {
        stationButtons_.value(stationId)->setEnabled(true);
        stationButtons_.value(stationId)->setText(QStringLiteral("查看电桩"));
    }
    setState(QStringLiteral("已加载 %1 个充电桩").arg(detail.chargers.size()));
    QTimer::singleShot(0, this, [this, detail] {
        StationDetailDialog dialog(detail, session_.user().id, facade_, this,
                                   originLongitude(), originLatitude(), true,
                                   session_.user().balance);
        bool recharge = false;
        connect(&dialog, &StationDetailDialog::rechargeRequested, &dialog, [&] {
            recharge = true;
            dialog.accept();
        });
        dialog.exec();
        if (recharge) openProfile();
        refreshStations();
        facade_.requestProfile();
    });
}

double UserMainWindow::originLongitude() const
{
    if (hasLocatedOrigin_) return locatedOrigin_.x();
    return regionBox_->currentData().toPointF().x();
}

double UserMainWindow::originLatitude() const
{
    if (hasLocatedOrigin_) return locatedOrigin_.y();
    return regionBox_->currentData().toPointF().y();
}

void UserMainWindow::setState(const QString &message, bool error)
{
    stateLabel_->setObjectName(error ? QStringLiteral("statusError")
                                     : QStringLiteral("statusLoading"));
    stateLabel_->style()->unpolish(stateLabel_);
    stateLabel_->style()->polish(stateLabel_);
    stateLabel_->setText(message);
}

}  // namespace ncs
