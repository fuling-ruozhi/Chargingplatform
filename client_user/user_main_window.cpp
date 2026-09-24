#include "user_main_window.h"

#include "charging_dialog.h"
#include "reminder_banner.h"
#include "reminder_controller.h"
#include "forecast_card.h"
#include "service/user_client_facade.h"
#include "station_detail_dialog.h"
#include "station_sorting.h"
#include "user_preference_dialog.h"
#include "user_profile_dialog.h"
#include "user_messages.h"

#include <QFrame>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace ncs {

UserMainWindow::UserMainWindow(const User &user, UserClientFacade &facade, QWidget *parent)
    : QMainWindow(parent), session_(user), facade_(facade)
{
    setWindowTitle(QStringLiteral("NCS 用户主页"));
    // 宽度补偿竖向滚动条占用，使内容区保持约 420px 的原始卡片宽度
    setFixedSize(436, 760);

    // 整页纵向滚动：内容（预测卡/推荐/电站列表）超出视口时可上下拖动
    auto *pageScroll = new QScrollArea(this);
    pageScroll->setObjectName(QStringLiteral("pageScroll"));
    pageScroll->setWidgetResizable(true);
    pageScroll->setFrameShape(QFrame::NoFrame);
    pageScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setCentralWidget(pageScroll);

    auto *page = new QWidget(pageScroll);
    page->setObjectName(QStringLiteral("userRoot"));
    pageScroll->setWidget(page);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 10);
    layout->setSpacing(9);

    auto *header = new QFrame(page);
    header->setObjectName(QStringLiteral("marketHeader"));
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(16, 13, 16, 13);
    headerLayout->setSpacing(3);
    auto *brand = new QLabel(QStringLiteral("NCS 充电"), header);
    brand->setObjectName(QStringLiteral("sectionTitle"));
    auto *accountRow = new QHBoxLayout;
    welcomeLabel_ = new QLabel(header);
    welcomeLabel_->setObjectName(QStringLiteral("mutedLabel"));
    balanceLabel_ = new QLabel(header);
    balanceLabel_->setObjectName(QStringLiteral("priceLabel"));
    auto *profile = new QPushButton(QStringLiteral("个人中心"), header);
    profile->setObjectName(QStringLiteral("profileButton"));
    profile->setFixedWidth(88);
    accountRow->addWidget(welcomeLabel_);
    accountRow->addStretch();
    accountRow->addWidget(balanceLabel_);
    accountRow->addWidget(profile);
    headerLayout->addWidget(brand);
    headerLayout->addLayout(accountRow);
    layout->addWidget(header);

    // EXT-SC-03 续航预测卡片（充电入口）
    layout->addWidget(new ForecastCard(facade_, page));

    auto *hero = new QFrame(page);
    hero->setObjectName(QStringLiteral("gradientHero"));
    hero->setFixedHeight(98);
    auto *heroLayout = new QVBoxLayout(hero);
    heroLayout->setContentsMargins(18, 15, 18, 15);
    heroLayout->setSpacing(3);
    auto *heroTitle = new QLabel(QStringLiteral("附近充电站"), hero);
    heroTitle->setObjectName(QStringLiteral("heroTitle"));
    auto *heroHint = new QLabel(QStringLiteral("实时查找空闲电桩，选择合适电站开始充电"), hero);
    heroHint->setObjectName(QStringLiteral("heroSubtitle"));
    heroHint->setWordWrap(true);
    heroLayout->addStretch();
    heroLayout->addWidget(heroTitle);
    heroLayout->addWidget(heroHint);
    layout->addWidget(hero);

    auto *titleRow = new QHBoxLayout;
    auto *refresh = new QPushButton(QStringLiteral("刷新"), page);
    refresh->setObjectName(QStringLiteral("pillButton"));
    refresh->setFixedWidth(66);
    regionBox_ = new QComboBox(page);
    regionBox_->addItem(QStringLiteral("北理工校区"), QPointF(116.3220, 39.9623));
    regionBox_->addItem(QStringLiteral("中关村"), QPointF(116.3168, 39.9836));
    regionBox_->addItem(QStringLiteral("亦庄"), QPointF(116.5062, 39.7951));
    sortBox_ = new QComboBox(page);
    sortBox_->addItem(QStringLiteral("距离优先"), static_cast<int>(StationSortMode::Distance));
    sortBox_->addItem(QStringLiteral("推荐优先"), static_cast<int>(StationSortMode::Recommendation));
    sortBox_->addItem(QStringLiteral("评分优先"), static_cast<int>(StationSortMode::Rating));
    sortBox_->setToolTip(QStringLiteral("选择电站排序方式"));
    titleRow->addWidget(regionBox_, 1);
    titleRow->addWidget(sortBox_);
    titleRow->addWidget(refresh);
    layout->addLayout(titleRow);

    auto *locationRow = new QHBoxLayout;
    locationRow->setSpacing(8);
    addressEdit_ = new QLineEdit(page);
    addressEdit_->setPlaceholderText(QStringLiteral("输入当前位置/地址"));
    addressEdit_->setClearButtonEnabled(true);
    auto *locate = new QPushButton(QStringLiteral("定位"), page);
    locate->setObjectName(QStringLiteral("secondaryButton"));
    locate->setFixedWidth(72);
    locationRow->addWidget(addressEdit_, 1);
    locationRow->addWidget(locate);
    layout->addLayout(locationRow);

    stateLabel_ = new QLabel(QStringLiteral("正在加载附近电站…"), page);
    stateLabel_->setObjectName(QStringLiteral("statusLoading"));
    stateLabel_->setWordWrap(true);
    layout->addWidget(stateLabel_);
    reminderBanner_ = new ReminderBanner(page);
    layout->addWidget(reminderBanner_);

    recommendationPanel_ = new QFrame(page);
    recommendationPanel_->setObjectName(QStringLiteral("recommendationPanel"));
    auto *recommendationRoot = new QVBoxLayout(recommendationPanel_);
    recommendationRoot->setContentsMargins(12, 11, 12, 11);
    recommendationRoot->setSpacing(7);
    auto *recommendationTop = new QHBoxLayout;
    auto *recommendationTitle = new QLabel(QStringLiteral("智能推荐 Top 3"),
                                           recommendationPanel_);
    recommendationTitle->setObjectName(QStringLiteral("sectionTitle"));
    socInput_ = new QDoubleSpinBox(recommendationPanel_);
    socInput_->setObjectName(QStringLiteral("socRecommendInput"));
    socInput_->setRange(0.0, 100.0);
    socInput_->setDecimals(0);
    socInput_->setValue(50.0);
    socInput_->setSuffix(QStringLiteral(" %"));
    socInput_->setFixedWidth(84);
    auto *recommendRefresh = new QPushButton(QStringLiteral("推荐"), recommendationPanel_);
    recommendRefresh->setObjectName(QStringLiteral("pillButton"));
    recommendRefresh->setFixedWidth(66);
    recommendationTop->addWidget(recommendationTitle);
    recommendationTop->addStretch();
    recommendationTop->addWidget(socInput_);
    recommendationTop->addWidget(recommendRefresh);
    recommendationLayout_ = new QVBoxLayout;
    recommendationLayout_->setContentsMargins(0, 0, 0, 0);
    recommendationLayout_->setSpacing(6);
    recommendationRoot->addLayout(recommendationTop);
    recommendationRoot->addLayout(recommendationLayout_);
    layout->addWidget(recommendationPanel_);

    // 电站列表直接并入整页滚动，不再使用嵌套滚动区
    auto *stationContainer = new QWidget(page);
    stationContainer->setObjectName(QStringLiteral("stationCatalog"));
    stationLayout_ = new QVBoxLayout(stationContainer);
    stationLayout_->setContentsMargins(0, 0, 4, 0);
    stationLayout_->setSpacing(10);
    stationLayout_->addStretch();
    layout->addWidget(stationContainer);

    connect(refresh, &QPushButton::clicked, this, &UserMainWindow::refreshStations);
    connect(recommendRefresh, &QPushButton::clicked,
            this, [this] {
                facade_.requestStationRecommendations(
                    originLongitude(), originLatitude(), socInput_->value());
            });
    connect(regionBox_, &QComboBox::currentIndexChanged,
            this, [this] {
                hasLocatedOrigin_ = false;
                refreshStations();
            });
    connect(sortBox_, &QComboBox::currentIndexChanged, this,
            [this] { renderStations(); });
    connect(locate, &QPushButton::clicked, this, &UserMainWindow::locateAddress);
    connect(addressEdit_, &QLineEdit::returnPressed, this, &UserMainWindow::locateAddress);
    connect(profile, &QPushButton::clicked, this, &UserMainWindow::openProfile);
    connect(&facade_, &UserClientFacade::stationsReceived,
            this, &UserMainWindow::showStations);
    connect(&facade_, &UserClientFacade::stationRecommendationsReceived,
            this, &UserMainWindow::showRecommendations);
    connect(&facade_, &UserClientFacade::profileReceived,
            this, &UserMainWindow::updateUser);
    connect(&facade_, &UserClientFacade::activeChargeReceived,
            this, &UserMainWindow::recoverActiveOrder);
    reminderController_ = new ReminderController(facade_, this);
    connect(&facade_, &UserClientFacade::preferenceReceived, this,
            [this](const UserPreference &preference) {
                reminderController_->setEnabled(preference.enabled);
            });
    connect(reminderController_, &ReminderController::notificationRequested,
            this, [this](const QVector<ReminderMatch> &matches) {
                reminderBanner_->showMatches(matches);
            });
    connect(&facade_, &UserClientFacade::requestFailed, this,
            [this](const QString &route, int, const QString &message) {
                if (route == QStringLiteral("station.list")) {
                    setState(userFacingError(message), true);
                } else if (route == QStringLiteral("station.recommend")) {
                    setState(QStringLiteral("推荐暂不可用：%1")
                                 .arg(userFacingError(message)), true);
                } else if (route == QStringLiteral("charge.active")
                           && initialRecoveryPending_) {
                    initialRecoveryPending_ = false;
                    setState(QStringLiteral("活动订单恢复失败：%1")
                                 .arg(userFacingError(message)), true);
                } else if (route == QStringLiteral("station.detail")
                           && pendingOpenStationId_ != 0) {
                    const qint64 stationId = pendingOpenStationId_;
                    pendingOpenStationId_ = 0;
                    if (stationButtons_.contains(stationId)) {
                        stationButtons_.value(stationId)->setEnabled(true);
                        stationButtons_.value(stationId)->setText(QStringLiteral("查看电桩"));
                    }
                    setState(userFacingError(message), true);
                }
            });
    connect(&facade_, &UserClientFacade::networkError, this,
            [this](const QString &message) {
                initialRecoveryPending_ = false;
                if (pendingOpenStationId_ != 0) {
                    const qint64 stationId = pendingOpenStationId_;
                    pendingOpenStationId_ = 0;
                    if (stationButtons_.contains(stationId)) {
                        stationButtons_.value(stationId)->setEnabled(true);
                        stationButtons_.value(stationId)->setText(QStringLiteral("查看电桩"));
                    }
                }
                setState(userFacingError(message), true);
            });
    connect(&facade_, &UserClientFacade::stationDetailReceived,
            this, &UserMainWindow::showStationDetail);
    updateUser(user);
    refreshStations();
    facade_.requestPreference();
    facade_.requestActiveCharge(session_.user().id);
}

void UserMainWindow::recoverActiveOrder(bool hasActive,
                                        const ChargingRecord &record)
{
    if (!initialRecoveryPending_) return;
    initialRecoveryPending_ = false;
    if (!hasActive
        || (record.status != ChargingOrderStatus::Reserved
            && record.status != ChargingOrderStatus::Charging)) return;

    setState(record.status == ChargingOrderStatus::Reserved
                 ? QStringLiteral("已恢复未完成的充电预约")
                 : QStringLiteral("已恢复正在进行的充电订单"));
    QTimer::singleShot(0, this, [this, record] {
        auto *dialog = new ChargingDialog(record, record.chargerCode,
                                          record.price, facade_, this);
        dialog->setObjectName(QStringLiteral("recoveredChargingDialog"));
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &QDialog::finished, this,
                [this] {
                    refreshStations();
                    facade_.requestProfile();
                });
        dialog->open();
    });
}

void UserMainWindow::openProfile()
{
    UserProfileDialog dialog(session_.user(), facade_, this);
    connect(&dialog, &UserProfileDialog::userChanged,
            this, &UserMainWindow::updateUser);
    connect(&dialog, &UserProfileDialog::logoutRequested,
            this, &UserMainWindow::logoutRequested);
    connect(&dialog, &UserProfileDialog::preferenceRequested,
            this, &UserMainWindow::openPreference);
    dialog.exec();
}

void UserMainWindow::openPreference()
{
    UserPreferenceDialog dialog(facade_, hasLocatedOrigin_, locatedOrigin_.x(),
                                 locatedOrigin_.y(), stations_, this);
    connect(&dialog, &UserPreferenceDialog::preferenceSaved, this,
            [this](const UserPreference &preference) {
                reminderController_->setEnabled(preference.enabled);
            });
    dialog.exec();
}

void UserMainWindow::updateUser(const User &user)
{
    session_.updateUser(user);
    welcomeLabel_->setText(QStringLiteral("你好，%1").arg(user.nickname));
    balanceLabel_->setText(QStringLiteral("¥%1").arg(user.balance, 0, 'f', 2));
}

void UserMainWindow::refreshStations()
{
    pendingOpenStationId_ = 0;
    setState(QStringLiteral("正在加载附近电站…"));
    facade_.requestStations(originLongitude(), originLatitude());
    facade_.requestStationRecommendations(
        originLongitude(), originLatitude(), socInput_->value());
}

void UserMainWindow::locateAddress()
{
    const QString address = addressEdit_->text().trimmed();
    if (address.isEmpty()) {
        hasLocatedOrigin_ = false;
        setState(QStringLiteral("请输入当前位置或选择演示区域"), true);
        return;
    }

    struct DemoLocation {
        QString keyword;
        QPointF coordinate;
    };
    const QVector<DemoLocation> demoLocations{
        {QStringLiteral("北理工"), QPointF(116.3220, 39.9623)},
        {QStringLiteral("北京理工"), QPointF(116.3220, 39.9623)},
        {QStringLiteral("中关村"), QPointF(116.3168, 39.9836)},
        {QStringLiteral("亦庄"), QPointF(116.5062, 39.7951)}};
    for (const DemoLocation &location : demoLocations) {
        if (!address.contains(location.keyword)) continue;
        locatedOrigin_ = location.coordinate;
        hasLocatedOrigin_ = true;
        setState(QStringLiteral("已定位到：%1").arg(location.keyword));
        refreshStations();
        return;
    }

    hasLocatedOrigin_ = false;
    setState(QStringLiteral("定位失败，已继续使用当前演示区域排序"), true);
    refreshStations();
}

void UserMainWindow::showStations(const QVector<Station> &stations)
{
    stations_ = stations;
    renderStations();
}

/*
 * Station cards deliberately keep the existing station/button maps and signal
 * wiring. Only their presentation changes from a vertical test list to a
 * compact two-column marketplace catalog.
 */

}  // namespace ncs
