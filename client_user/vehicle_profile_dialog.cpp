#include "vehicle_profile_dialog.h"

#include "service/user_client_facade.h"
#include "user_messages.h"

#include <QDoubleSpinBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStyle>
#include <QTime>
#include <QTimeEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace ncs {

namespace {

QWidget *rowWithLabel(const QString &caption, QWidget *editor, QWidget *parent)
{
    auto *row = new QHBoxLayout;
    auto *label = new QLabel(caption, parent);
    label->setObjectName(QStringLiteral("mutedLabel"));
    row->addWidget(label);
    row->addStretch();
    row->addWidget(editor);
    auto *wrapper = new QWidget(parent);
    wrapper->setLayout(row);
    return wrapper;
}

}

VehicleProfileDialog::VehicleProfileDialog(UserClientFacade &facade, QWidget *parent)
    : QDialog(parent), facade_(facade)
{
    setWindowTitle(QStringLiteral("车辆档案"));
    setFixedSize(410, 660);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("车辆档案"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    auto *subtitle = new QLabel(
        QStringLiteral("一次性设置车辆与充电偏好，保存后用于充电建议"), this);
    subtitle->setObjectName(QStringLiteral("mutedLabel"));
    subtitle->setWordWrap(true);
    layout->addWidget(title);
    layout->addWidget(subtitle);

    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("infoCard"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(18, 16, 18, 16);
    cardLayout->setSpacing(10);

    capacity_ = new QSpinBox(this);
    capacity_->setRange(1, 200);
    capacity_->setValue(60);
    capacity_->setSuffix(QStringLiteral(" kWh"));
    cardLayout->addWidget(rowWithLabel(QStringLiteral("电池容量"), capacity_, card));

    targetSoc_ = new QSpinBox(this);
    targetSoc_->setRange(10, 100);
    targetSoc_->setValue(80);
    targetSoc_->setSuffix(QStringLiteral(" %"));
    cardLayout->addWidget(rowWithLabel(QStringLiteral("目标 SoC"), targetSoc_, card));

    reserve_ = new QDoubleSpinBox(this);
    reserve_->setRange(0.0, 100000.0);
    reserve_->setDecimals(2);
    reserve_->setValue(5.0);
    reserve_->setPrefix(QStringLiteral("¥ "));
    cardLayout->addWidget(rowWithLabel(QStringLiteral("余额保留值"), reserve_, card));

    leaveTime_ = new QTimeEdit(this);
    leaveTime_->setDisplayFormat(QStringLiteral("HH:mm"));
    leaveTime_->setTime(QTime(18, 0));
    cardLayout->addWidget(rowWithLabel(QStringLiteral("常用离开时间"), leaveTime_, card));

    auto *modeLabel = new QLabel(QStringLiteral("充电模式"), this);
    modeLabel->setObjectName(QStringLiteral("mutedLabel"));
    cardLayout->addWidget(modeLabel);
    economy_ = new QRadioButton(QStringLiteral("省钱模式（低功率慢充，费用优先）"), this);
    balanced_ = new QRadioButton(QStringLiteral("均衡模式（标准功率）"), this);
    fast_ = new QRadioButton(QStringLiteral("快速模式（满功率快充，时间优先）"), this);
    balanced_->setChecked(true);
    cardLayout->addWidget(economy_);
    cardLayout->addWidget(balanced_);
    cardLayout->addWidget(fast_);
    layout->addWidget(card);

    stateLabel_ = new QLabel(this);
    stateLabel_->setObjectName(QStringLiteral("statusLoading"));
    stateLabel_->setAlignment(Qt::AlignCenter);
    stateLabel_->setWordWrap(true);
    layout->addWidget(stateLabel_);

    saveButton_ = new QPushButton(QStringLiteral("保存档案"), this);
    saveButton_->setDefault(true);
    layout->addWidget(saveButton_);
    auto *backButton = new QPushButton(QStringLiteral("返回"), this);
    backButton->setObjectName(QStringLiteral("ghostButton"));
    connect(backButton, &QPushButton::clicked, this, &QDialog::reject);
    layout->addWidget(backButton);
    layout->addStretch();

    requestTimer_ = new QTimer(this);
    requestTimer_->setSingleShot(true);
    requestTimer_->setInterval(10000);
    connect(requestTimer_, &QTimer::timeout, this, [this] {
        finishRequest(QStringLiteral("请求超时，请稍后重试"), true);
    });
    connect(saveButton_, &QPushButton::clicked, this, [this] {
        if (!pendingRoute_.isEmpty()) return;
        beginRequest(QStringLiteral("user.vehicle.profile.update"),
                     QStringLiteral("正在保存档案…"));
        facade_.updateVehicleProfile(collectProfile());
    });
    connect(&facade_, &UserClientFacade::vehicleProfileReceived,
            this, [this](const VehicleProfile &profile) {
                applyProfile(profile);
                finishRequest(QStringLiteral("档案已加载"), false);
            });
    connect(&facade_, &UserClientFacade::vehicleProfileUpdated,
            this, [this](const VehicleProfile &) {
                finishRequest(QStringLiteral("保存成功"), false);
            });
    connect(&facade_, &UserClientFacade::requestFailed,
            this, [this](const QString &route, int, const QString &message) {
                if (route == pendingRoute_) finishRequest(userFacingError(message), true);
            });
    connect(&facade_, &UserClientFacade::networkError,
            this, [this](const QString &message) {
                if (!pendingRoute_.isEmpty()) finishRequest(userFacingError(message), true);
            });

    beginRequest(QStringLiteral("user.vehicle.profile.get"),
                 QStringLiteral("正在读取档案…"));
    facade_.requestVehicleProfile();
}

void VehicleProfileDialog::applyProfile(const VehicleProfile &profile)
{
    capacity_->setValue(qRound(profile.batteryCapacityKwh));
    targetSoc_->setValue(qRound(profile.targetSoc));
    reserve_->setValue(profile.minBalanceReserve);
    leaveTime_->setTime(QTime::fromString(
        profile.usualLeaveTime.isEmpty() ? QStringLiteral("18:00")
                                         : profile.usualLeaveTime,
        QStringLiteral("HH:mm")));
    economy_->setChecked(profile.chargeMode == static_cast<int>(ChargeMode::Economy));
    balanced_->setChecked(profile.chargeMode == static_cast<int>(ChargeMode::Balanced));
    fast_->setChecked(profile.chargeMode == static_cast<int>(ChargeMode::Fast));
}

VehicleProfile VehicleProfileDialog::collectProfile() const
{
    VehicleProfile profile;
    profile.batteryCapacityKwh = capacity_->value();
    profile.targetSoc = targetSoc_->value();
    profile.minBalanceReserve = reserve_->value();
    profile.usualLeaveTime = leaveTime_->time().toString(QStringLiteral("HH:mm"));
    if (economy_->isChecked()) {
        profile.chargeMode = static_cast<int>(ChargeMode::Economy);
    } else if (fast_->isChecked()) {
        profile.chargeMode = static_cast<int>(ChargeMode::Fast);
    } else {
        profile.chargeMode = static_cast<int>(ChargeMode::Balanced);
    }
    return profile;
}

void VehicleProfileDialog::beginRequest(const QString &route, const QString &message)
{
    pendingRoute_ = route;
    saveButton_->setEnabled(false);
    stateLabel_->setObjectName(QStringLiteral("statusLoading"));
    stateLabel_->style()->unpolish(stateLabel_);
    stateLabel_->style()->polish(stateLabel_);
    stateLabel_->setText(message);
    requestTimer_->start();
}

void VehicleProfileDialog::finishRequest(const QString &message, bool error)
{
    requestTimer_->stop();
    pendingRoute_.clear();
    saveButton_->setEnabled(true);
    stateLabel_->setObjectName(error ? QStringLiteral("statusError")
                                     : QStringLiteral("statusSuccess"));
    stateLabel_->style()->unpolish(stateLabel_);
    stateLabel_->style()->polish(stateLabel_);
    stateLabel_->setText(message);
}

}
