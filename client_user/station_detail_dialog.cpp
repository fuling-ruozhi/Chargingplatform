#include "station_detail_dialog.h"
#include "charging_dialog.h"
#include "confirm_action_dialog.h"
#include "model/business_error.h"
#include "navigation_launcher.h"
#include "service/user_client_facade.h"
#include "util/navigation_url.h"
#include "user_messages.h"

#ifdef NCS_HAS_WEBENGINE
#include "map_view_dialog.h"
#endif

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

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

StationDetailDialog::StationDetailDialog(const StationDetail &detail, qint64 userId,
                                         UserClientFacade &facade, QWidget *parent,
                                         double originLongitude, double originLatitude,
                                         bool hasOrigin)
    : StationDetailDialog(detail, userId, facade, parent, originLongitude,
                          originLatitude, hasOrigin, 0.0)
{}

StationDetailDialog::StationDetailDialog(const StationDetail &detail, qint64 userId,
                                         UserClientFacade &facade, QWidget *parent,
                                         double originLongitude, double originLatitude,
                                         bool hasOrigin, double userBalance)
    : QDialog(parent), detail_(detail), userId_(userId), facade_(facade),
      originLongitude_(originLongitude), originLatitude_(originLatitude),
      hasOrigin_(hasOrigin), userBalance_(userBalance)
{
    setWindowTitle(QStringLiteral("充电站详情"));
    setFixedSize(440, 700);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 12);
    layout->setSpacing(9);

    auto *summary = new QFrame(this);
    summary->setObjectName(QStringLiteral("gradientHero"));
    auto *summaryLayout = new QVBoxLayout(summary);
    summaryLayout->setContentsMargins(18, 15, 18, 15);
    summaryLayout->setSpacing(4);
    auto *name = new QLabel(detail_.station.name, summary);
    name->setObjectName(QStringLiteral("heroTitle"));
    auto *address = new QLabel(detail_.station.address, summary);
    address->setObjectName(QStringLiteral("heroSubtitle"));
    address->setWordWrap(true);
    auto *price = new QLabel(QStringLiteral("%1 元/kWh")
                                 .arg(detail_.station.price, 0, 'f', 2), summary);
    price->setObjectName(QStringLiteral("heroTitle"));
    auto *location = new QLabel(
        QStringLiteral("距当前位置 %1 km")
            .arg(detail_.station.distanceKm, 0, 'f', 2), summary);
    location->setObjectName(QStringLiteral("heroSubtitle"));
    auto *summaryBottom = new QHBoxLayout;
    summaryBottom->addWidget(price);
    summaryBottom->addStretch();
    summaryBottom->addWidget(location);
    summaryLayout->addWidget(name);
    summaryLayout->addWidget(address);
    summaryLayout->addLayout(summaryBottom);
    layout->addWidget(summary);

    requestStateLabel_ = new QLabel(QStringLiteral("正在确认充电桩状态…"), this);
    requestStateLabel_->setObjectName(QStringLiteral("statusLoading"));
    requestStateLabel_->setWordWrap(true);
    auto *sectionRow = new QHBoxLayout;
    auto *sectionTitle = new QLabel(QStringLiteral("可用充电桩"), this);
    sectionTitle->setObjectName(QStringLiteral("sectionTitle"));
    auto *navigation = new QPushButton(QStringLiteral("导航到这里"), this);
    navigation->setObjectName(QStringLiteral("pillButton"));
    navigation->setEnabled(hasOrigin_);
    connect(navigation, &QPushButton::clicked,
            this, &StationDetailDialog::openNavigation);
    sectionRow->addWidget(sectionTitle);
    sectionRow->addStretch();
    sectionRow->addWidget(navigation);
#ifdef NCS_HAS_WEBENGINE
    auto *mapButton = new QPushButton(QStringLiteral("站内地图"), this);
    mapButton->setObjectName(QStringLiteral("secondaryButton"));
    connect(mapButton, &QPushButton::clicked,
            this, &StationDetailDialog::openMapView);
    sectionRow->addWidget(mapButton);
#endif
    favoriteButton_ = new QPushButton(QStringLiteral("☆ 关注站点"), this);
    favoriteButton_->setObjectName(QStringLiteral("secondaryButton"));
    layout->addWidget(favoriteButton_);
    auto *ratingCard = new QFrame(this);
    ratingCard->setObjectName(QStringLiteral("infoCard"));
    auto *ratingLayout = new QVBoxLayout(ratingCard);
    ratingLayout->setContentsMargins(13, 10, 13, 10);
    auto *ratingTitle = new QLabel(QStringLiteral("用户评价"), ratingCard);
    ratingTitle->setObjectName(QStringLiteral("sectionTitle"));
    ratingLabel_ = new QLabel(ratingCard);
    ratingLabel_->setObjectName(QStringLiteral("ratingDetails"));
    ratingLabel_->setWordWrap(true);
    ratingLayout->addWidget(ratingTitle);
    ratingLayout->addWidget(ratingLabel_);
    layout->addWidget(ratingCard);
    renderRatingSummary();
    layout->addLayout(sectionRow);
    layout->addWidget(requestStateLabel_);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto *container = new QWidget(scroll);
    chargerLayout_ = new QVBoxLayout(container);
    chargerLayout_->setContentsMargins(0, 0, 4, 0);
    chargerLayout_->setSpacing(9);
    scroll->setWidget(container);
    layout->addWidget(scroll, 1);

    connect(&facade_, &UserClientFacade::stationDetailReceived, this,
            [this](const StationDetail &updated) {
                if (!awaitingDetail_ || updated.station.id != detail_.station.id) return;
                detail_ = updated;
                renderRatingSummary();
                awaitingDetail_ = false;
                finishRefreshIfReady();
            });
    connect(&facade_, &UserClientFacade::activeChargeReceived, this,
            [this](bool hasActive, const ChargingRecord &record) {
                if (!awaitingActive_) return;
                hasActive_ = hasActive;
                activeRecord_ = hasActive ? record : ChargingRecord();
                awaitingActive_ = false;
                finishRefreshIfReady();
            });
    connect(&facade_, &UserClientFacade::reservationCreated, this,
            [this](const ChargingRecord &record) {
                if (pendingRoute_ != QStringLiteral("charge.reserve")
                    || record.chargerId != pendingChargerId_) return;
                pendingRoute_.clear();
                pendingChargerId_ = 0;
                openCharging(record);
            });
    connect(&facade_, &UserClientFacade::chargeStarted, this,
            [this](const ChargingRecord &record) {
                if (pendingRoute_ != QStringLiteral("charge.start")
                    || record.chargerId != pendingChargerId_) return;
                pendingRoute_.clear();
                pendingChargerId_ = 0;
                openCharging(record);
            });
    connect(&facade_, &UserClientFacade::reservationCancelled, this,
            [this](const ChargingRecord &record) {
                if (pendingRoute_ != QStringLiteral("charge.cancel")
                    || record.chargerId != pendingChargerId_) return;
                pendingRoute_.clear();
                pendingChargerId_ = 0;
                if (confirmation_) confirmation_->finishSuccess();
                requestCurrentState(true);
            });
    connect(&facade_, &UserClientFacade::favoriteStationsReceived, this,
            [this](const QVector<qint64> &stationIds) {
                if (favoriteRequestPending_) {
                    favoriteRequestPending_ = false;
                    updateFavoriteButton(stationIds.contains(detail_.station.id));
                }
            });
    connect(&facade_, &UserClientFacade::favoriteStationChanged, this,
            [this](qint64 stationId, bool added) {
                if (stationId != detail_.station.id) return;
                favoriteRequestPending_ = false;
                updateFavoriteButton(added);
            });
    connect(favoriteButton_, &QPushButton::clicked, this, [this] {
        if (favoriteRequestPending_) return;
        favoriteRequestPending_ = true;
        favoriteButton_->setEnabled(false);
        if (favorite_) facade_.removeFavoriteStation(detail_.station.id);
        else facade_.addFavoriteStation(detail_.station.id);
    });
    connect(&facade_, &UserClientFacade::requestFailed, this,
            [this](const QString &route, int code, const QString &message) {
                if (route == QStringLiteral("favorite_station.list")
                    || route == QStringLiteral("favorite_station.add")
                    || route == QStringLiteral("favorite_station.remove")) {
                    favoriteRequestPending_ = false;
                    favoriteButton_->setEnabled(true);
                    if (route != QStringLiteral("favorite_station.list")) {
                        requestStateLabel_->setText(userFacingError(message));
                    }
                    return;
                }
                if (route == pendingRoute_) {
                    const QString display = userFacingError(message);
                    rechargeButton_->setVisible(
                        code == static_cast<int>(BusinessErrorCode::InsufficientBalance));
                    if (confirmation_) confirmation_->finishFailure(display);
                    resetAction(display);
                    return;
                }
                if ((route == QStringLiteral("station.detail") && awaitingDetail_)
                    || (route == QStringLiteral("charge.active") && awaitingActive_)) {
                    awaitingDetail_ = false;
                    awaitingActive_ = false;
                    resetAction(userFacingError(message));
                }
            });
    connect(&facade_, &UserClientFacade::networkError, this,
            [this](const QString &message) {
                if (favoriteRequestPending_) {
                    favoriteRequestPending_ = false;
                    favoriteButton_->setEnabled(true);
                }
                if (!pendingRoute_.isEmpty() || awaitingDetail_ || awaitingActive_) {
                    const QString display = userFacingError(message);
                    awaitingDetail_ = false;
                    awaitingActive_ = false;
                    if (confirmation_) confirmation_->finishFailure(display);
                    resetAction(display);
                }
            });

    rechargeButton_ = new QPushButton(QStringLiteral("余额不足，去充值"), this);
    rechargeButton_->setObjectName(QStringLiteral("secondaryButton"));
    rechargeButton_->hide();
    connect(rechargeButton_, &QPushButton::clicked, this, [this] {
        emit rechargeRequested();
    });
    layout->addWidget(rechargeButton_);
    auto *backButton = new QPushButton(QStringLiteral("返回电站列表"), this);
    backButton->setObjectName(QStringLiteral("ghostButton"));
    connect(backButton, &QPushButton::clicked, this, &QDialog::reject);
    layout->addWidget(backButton);

    favoriteRequestPending_ = true;
    facade_.requestFavoriteStations();
    requestCurrentState(false);
}

void StationDetailDialog::updateFavoriteButton(bool favorite)
{
    favorite_ = favorite;
    favoriteButton_->setEnabled(true);
    favoriteButton_->setText(favorite ? QStringLiteral("★ 已关注")
                                      : QStringLiteral("☆ 关注站点"));
}

void StationDetailDialog::renderRatingSummary()
{
    if (detail_.ratingSummary.reviewCount <= 0) {
        ratingLabel_->setText(QStringLiteral("暂无用户评价"));
        return;
    }
    const StationRatingSummary &summary = detail_.ratingSummary;
    ratingLabel_->setText(
        QStringLiteral("综合评分  %1 / 5.0    %2 人评价\n"
                       "环境 %3    排队 %4\n设备 %5    停车 %6")
            .arg(summary.averageScore, 0, 'f', 1)
            .arg(summary.reviewCount)
            .arg(summary.environmentAverage, 0, 'f', 1)
            .arg(summary.queueAverage, 0, 'f', 1)
            .arg(summary.equipmentAverage, 0, 'f', 1)
            .arg(summary.parkingAverage, 0, 'f', 1));
}

void StationDetailDialog::requestCurrentState(bool refreshDetail)
{
    pendingRoute_.clear();
    pendingChargerId_ = 0;
    awaitingDetail_ = refreshDetail;
    awaitingActive_ = true;
    ++refreshGeneration_;
    const int generation = refreshGeneration_;
    refreshStyle(requestStateLabel_, QStringLiteral("statusLoading"));
    requestStateLabel_->setText(QStringLiteral("正在刷新充电桩状态…"));
    for (QPushButton *button : actionButtons_) button->setEnabled(false);
    if (refreshDetail) {
        if (hasOrigin_) {
            facade_.requestStationDetail(detail_.station.id, originLongitude_,
                                         originLatitude_);
        } else {
            facade_.requestStationDetail(detail_.station.id);
        }
    }
    facade_.requestActiveCharge(userId_);
    QTimer::singleShot(10000, this, [this, generation] {
        if (generation != refreshGeneration_ || (!awaitingDetail_ && !awaitingActive_)) return;
        awaitingDetail_ = false;
        awaitingActive_ = false;
        resetAction(QStringLiteral("刷新超时，请确认服务端正常运行后重试"));
    });
}

void StationDetailDialog::finishRefreshIfReady()
{
    if (awaitingDetail_ || awaitingActive_) return;
    ++refreshGeneration_;
    renderChargers();
}

}  // namespace ncs
