#include "user_profile_dialog.h"

#include "avatar_image_processor.h"
#include "avatar_source_dialog.h"
#include "camera_capture_dialog.h"
#include "confirm_action_dialog.h"
#include "order_history_dialog.h"
#include "forecast_card.h"
#include "service/user_client_facade.h"
#include "user_messages.h"
#include "vehicle_profile_dialog.h"

#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QMediaDevices>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

namespace ncs {

UserProfileDialog::UserProfileDialog(const User &user, UserClientFacade &facade,
                                     QWidget *parent,
                                     CameraDevicesProvider cameraDevices)
    : QDialog(parent), user_(user), facade_(facade),
      cameraDevices_(std::move(cameraDevices)),
      avatarLabel_(new QLabel(this)), phoneLabel_(new QLabel(this)),
      balanceLabel_(new QLabel(this)), createdLabel_(new QLabel(this)),
      stateLabel_(new QLabel(this)), nicknameEdit_(new QLineEdit(this)),
      amountEdit_(new QDoubleSpinBox(this)),
      nicknameButton_(new QPushButton(QStringLiteral("保存昵称"), this)),
      avatarButton_(new QPushButton(QStringLiteral("修改头像"), this)),
      rechargeButton_(new QPushButton(QStringLiteral("确认充值"), this)),
      logoutButton_(new QPushButton(QStringLiteral("退出登录"), this)),
      ordersButton_(new QPushButton(QStringLiteral("我的订单"), this)),
      preferenceButton_(new QPushButton(QStringLiteral("提醒偏好"), this)),
      requestTimer_(new QTimer(this))
{
    setWindowTitle(QStringLiteral("用户中心"));
    setFixedSize(390, 780);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("我的账户"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title);
    auto *profileCard = new QFrame(this);
    profileCard->setObjectName(QStringLiteral("profileCard"));
    auto *profileLayout = new QHBoxLayout(profileCard);
    profileLayout->setContentsMargins(16, 15, 16, 15);
    profileLayout->setSpacing(13);
    avatarLabel_->setFixedSize(68, 68);
    avatarLabel_->setAlignment(Qt::AlignCenter);
    avatarLabel_->setObjectName(QStringLiteral("avatarCircle"));
    avatarButton_->setObjectName(QStringLiteral("secondaryButton"));
    phoneLabel_->setObjectName(QStringLiteral("mutedLabel"));
    createdLabel_->setObjectName(QStringLiteral("mutedLabel"));
    balanceLabel_->setObjectName(QStringLiteral("primaryNumber"));
    nicknameEdit_->setObjectName(QStringLiteral("nicknameInput"));
    auto *identityLayout = new QVBoxLayout;
    identityLayout->setSpacing(3);
    identityLayout->addWidget(phoneLabel_);
    identityLayout->addWidget(createdLabel_);
    identityLayout->addWidget(balanceLabel_);
    profileLayout->addWidget(avatarLabel_);
    profileLayout->addLayout(identityLayout, 1);
    layout->addWidget(profileCard);

    // EXT-SC-03 续航预测卡片（个人中心）
    layout->addWidget(new ForecastCard(facade_, this));

    auto *editCard = new QFrame(this);
    editCard->setObjectName(QStringLiteral("settingsCard"));
    auto *editLayout = new QVBoxLayout(editCard);
    editLayout->setContentsMargins(18, 16, 18, 16);
    editLayout->addWidget(new QLabel(QStringLiteral("昵称"), editCard));
    auto *nicknameRow = new QHBoxLayout;
    nicknameEdit_->setMaxLength(20);
    nicknameRow->addWidget(nicknameEdit_, 1);
    nicknameRow->addWidget(nicknameButton_);
    editLayout->addLayout(nicknameRow);
    editLayout->addWidget(new QLabel(QStringLiteral("账户充值"), editCard));
    auto *rechargeRow = new QHBoxLayout;
    amountEdit_->setRange(0.01, 10000.00);
    amountEdit_->setObjectName(QStringLiteral("rechargeAmount"));
    amountEdit_->setDecimals(2);
    amountEdit_->setValue(100.00);
    amountEdit_->setPrefix(QStringLiteral("¥ "));
    rechargeRow->addWidget(amountEdit_, 1);
    rechargeRow->addWidget(rechargeButton_);
    editLayout->addLayout(rechargeRow);
    editLayout->addWidget(avatarButton_);
    layout->addWidget(editCard);

    auto *vehicleButton = new QPushButton(QStringLiteral("车辆档案"), this);
    vehicleButton->setObjectName(QStringLiteral("secondaryButton"));
    connect(vehicleButton, &QPushButton::clicked, this, [this] {
        VehicleProfileDialog dialog(facade_, this);
        dialog.exec();
    });
    layout->addWidget(vehicleButton);

    stateLabel_->setObjectName(QStringLiteral("statusLoading"));
    stateLabel_->setAlignment(Qt::AlignCenter);
    stateLabel_->setWordWrap(true);
    layout->addWidget(stateLabel_);
    ordersButton_->setObjectName(QStringLiteral("secondaryButton"));
    layout->addWidget(ordersButton_);
    preferenceButton_->setObjectName(QStringLiteral("secondaryButton"));
    layout->addWidget(preferenceButton_);
    logoutButton_->setObjectName(QStringLiteral("ghostButton"));
    layout->addWidget(logoutButton_);
    layout->addStretch();

    requestTimer_->setSingleShot(true);
    requestTimer_->setInterval(10000);
    connect(requestTimer_, &QTimer::timeout, this, [this] {
        finishRequest(QStringLiteral("请求超时，请稍后重试"), true);
    });
    connect(nicknameButton_, &QPushButton::clicked,
            this, &UserProfileDialog::updateNickname);
    connect(avatarButton_, &QPushButton::clicked,
            this, &UserProfileDialog::chooseAvatar);
    connect(rechargeButton_, &QPushButton::clicked,
            this, &UserProfileDialog::recharge);
    connect(ordersButton_, &QPushButton::clicked,
            this, &UserProfileDialog::openOrders);
    connect(preferenceButton_, &QPushButton::clicked,
            this, &UserProfileDialog::preferenceRequested);
    connect(logoutButton_, &QPushButton::clicked, this, [this] {
        if (!pendingRoute_.isEmpty() || confirmation_) return;
        auto *dialog = new ConfirmActionDialog(
            QStringLiteral("退出登录"),
            QStringLiteral("确认结束当前登录会话吗？未完成的充电订单仍会保留。"),
            QStringLiteral("确认退出"), ConfirmActionDialog::Severity::Warning,
            this);
        confirmation_ = dialog;
        connect(dialog, &QDialog::finished, this, [this, dialog] {
            confirmation_.clear();
            QTimer::singleShot(0, this, [dialog] { delete dialog; });
        });
        connect(dialog, &ConfirmActionDialog::confirmed, this, [this] {
            beginRequest(QStringLiteral("user.logout"), QStringLiteral("正在退出…"));
            facade_.logout();
        });
        dialog->open();
    });
    connect(&facade_, &UserClientFacade::profileReceived,
            this, [this](const User &value) {
                user_ = value;
                render();
                finishRequest(QStringLiteral("资料已更新"), false);
            });
    connect(&facade_, &UserClientFacade::profileUpdated,
            this, [this](const User &value) {
                user_ = value;
                render();
                emit userChanged(user_);
                finishRequest(QStringLiteral("保存成功"), false);
            });
    connect(&facade_, &UserClientFacade::rechargeSucceeded,
            this, [this](const RechargeResult &result) {
                if (confirmation_) confirmation_->finishSuccess();
                user_.balance = result.balanceAfter;
                render();
                emit userChanged(user_);
                finishRequest(QStringLiteral("充值成功，当前余额 ¥%1")
                                  .arg(result.balanceAfter, 0, 'f', 2), false);
            });
    connect(&facade_, &UserClientFacade::logoutSucceeded, this, [this] {
        requestTimer_->stop();
        if (confirmation_) confirmation_->finishSuccess();
        emit logoutRequested();
        accept();
    });
    connect(&facade_, &UserClientFacade::requestFailed, this,
            [this](const QString &route, int, const QString &message) {
                if (route != pendingRoute_) return;
                const QString display = userFacingError(message);
                if (confirmation_) confirmation_->finishFailure(display);
                finishRequest(display, true);
            });
    connect(&facade_, &UserClientFacade::networkError, this,
            [this](const QString &message) {
                if (pendingRoute_.isEmpty()) return;
                const QString display = userFacingError(message);
                if (confirmation_) confirmation_->finishFailure(display);
                finishRequest(display, true);
            });
    render();
    beginRequest(QStringLiteral("user.profile.get"), QStringLiteral("正在读取账户信息…"));
    facade_.requestProfile();
}

UserProfileDialog::~UserProfileDialog()
{
    clearPendingAvatar(true);
}

void UserProfileDialog::reject()
{
    if (!pendingRoute_.isEmpty()) {
        stateLabel_->setText(QStringLiteral("正在保存资料，请稍候…"));
        return;
    }
    QDialog::reject();
}

void UserProfileDialog::render()
{
    nicknameEdit_->setText(user_.nickname);
    phoneLabel_->setText(QStringLiteral("手机号：%1").arg(user_.phone));
    createdLabel_->setText(QStringLiteral("注册时间：%1").arg(user_.createdAt));
    balanceLabel_->setText(QStringLiteral("账户余额  ¥%1").arg(user_.balance, 0, 'f', 2));
    const QString root = QDir(QStandardPaths::writableLocation(
        QStandardPaths::GenericDataLocation)).filePath(QStringLiteral("NCS"));
    QPixmap avatar(QDir(root).filePath(user_.avatarPath));
    if (avatar.isNull()) {
        const QString initial = user_.nickname.trimmed().left(1);
        avatarLabel_->setText(initial.isEmpty() ? QStringLiteral("N") : initial);
    }
    else {
        avatarLabel_->setText(QString());
        avatarLabel_->setPixmap(avatar.scaled(72, 72, Qt::KeepAspectRatio,
                                              Qt::SmoothTransformation));
    }
}

void UserProfileDialog::chooseAvatar()
{
    if (!pendingRoute_.isEmpty() || avatarSourceDialog_ || cameraDialog_
        || cameraUnavailableDialog_) return;
    auto *dialog = new AvatarSourceDialog(this);
    avatarSourceDialog_ = dialog;
    connect(dialog, &AvatarSourceDialog::albumRequested,
            this, [this] { QTimer::singleShot(0, this, &UserProfileDialog::chooseAvatarFromAlbum); });
    connect(dialog, &AvatarSourceDialog::cameraRequested,
            this, [this] { QTimer::singleShot(0, this, &UserProfileDialog::chooseAvatarFromCamera); });
    connect(dialog, &QDialog::finished, this, [this, dialog] {
        avatarSourceDialog_.clear();
        dialog->deleteLater();
    });
    dialog->open();
}

void UserProfileDialog::chooseAvatarFromAlbum()
{
    if (!pendingRoute_.isEmpty()) return;
    const QString source = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择头像"), QString(),
        QStringLiteral("图片 (*.jpg *.jpeg *.png *.webp *.bmp)"));
    if (source.isEmpty()) return;
    applyAvatarFromFile(source);
}

void UserProfileDialog::chooseAvatarFromCamera()
{
    if (!pendingRoute_.isEmpty() || cameraDialog_) return;
    const QList<QCameraDevice> devices = cameraDevices_
        ? cameraDevices_() : QMediaDevices::videoInputs();
    if (devices.isEmpty()) {
        showNoCameraFallback();
        return;
    }
    auto *dialog = new CameraCaptureDialog(devices.first(), this);
    cameraDialog_ = dialog;
    connect(dialog, &CameraCaptureDialog::photoSelected,
            this, &UserProfileDialog::applyAvatarFromFile);
    connect(dialog, &QDialog::finished, this, [this, dialog] {
        cameraDialog_.clear();
        dialog->deleteLater();
    });
    dialog->open();
}

void UserProfileDialog::showNoCameraFallback()
{
    if (cameraUnavailableDialog_) return;
    auto *dialog = new CameraUnavailableDialog(this);
    cameraUnavailableDialog_ = dialog;
    connect(dialog, &CameraUnavailableDialog::albumRequested,
            this, [this] { QTimer::singleShot(0, this, &UserProfileDialog::chooseAvatarFromAlbum); });
    connect(dialog, &QDialog::finished, this, [this, dialog] {
        cameraUnavailableDialog_.clear();
        dialog->deleteLater();
    });
    dialog->open();
}

void UserProfileDialog::applyAvatarFromFile(const QString &sourcePath)
{
    if (!pendingRoute_.isEmpty() || sourcePath.isEmpty()) return;
    const AvatarImageResult result = AvatarImageProcessor::process(sourcePath, user_.id);
    if (!result.success) {
        finishRequest(result.error, true);
        return;
    }
    pendingAvatarAbsolutePath_ = result.absolutePath;
    beginRequest(QStringLiteral("user.profile.avatar.update"),
                 QStringLiteral("正在保存头像…"));
    facade_.updateAvatar(result.relativePath);
}

void UserProfileDialog::updateNickname()
{
    if (!pendingRoute_.isEmpty()) return;
    beginRequest(QStringLiteral("user.profile.nickname.update"),
                 QStringLiteral("正在保存昵称…"));
    facade_.updateNickname(nicknameEdit_->text());
}

void UserProfileDialog::recharge()
{
    if (!pendingRoute_.isEmpty() || confirmation_) return;
    const double amount = amountEdit_->value();
    auto *dialog = new ConfirmActionDialog(
        QStringLiteral("确认充值"),
        QStringLiteral("确认向当前账户充值 ¥%1 吗？")
            .arg(amount, 0, 'f', 2),
        QStringLiteral("确认充值"), ConfirmActionDialog::Severity::Warning,
        this);
    confirmation_ = dialog;
    connect(dialog, &QDialog::finished, this, [this, dialog] {
        confirmation_.clear();
        QTimer::singleShot(0, this, [dialog] { delete dialog; });
    });
    connect(dialog, &ConfirmActionDialog::confirmed, this, [this, amount] {
        beginRequest(QStringLiteral("user.recharge"), QStringLiteral("正在充值…"));
        facade_.recharge(amount);
    });
    dialog->open();
}

void UserProfileDialog::openOrders()
{
    if (!pendingRoute_.isEmpty()) return;
    OrderHistoryDialog dialog(facade_, this);
    dialog.exec();
}

void UserProfileDialog::beginRequest(const QString &route, const QString &message)
{
    pendingRoute_ = route;
    nicknameButton_->setEnabled(false);
    avatarButton_->setEnabled(false);
    rechargeButton_->setEnabled(false);
    logoutButton_->setEnabled(false);
    ordersButton_->setEnabled(false);
    preferenceButton_->setEnabled(false);
    stateLabel_->setObjectName(QStringLiteral("statusLoading"));
    stateLabel_->style()->unpolish(stateLabel_);
    stateLabel_->style()->polish(stateLabel_);
    stateLabel_->setText(message);
    requestTimer_->start(route == QStringLiteral("user.profile.avatar.update")
                             ? 12000 : 10000);
}

void UserProfileDialog::finishRequest(const QString &message, bool error)
{
    const bool avatarRequest = pendingRoute_
        == QStringLiteral("user.profile.avatar.update");
    requestTimer_->stop();
    pendingRoute_.clear();
    nicknameButton_->setEnabled(true);
    avatarButton_->setEnabled(true);
    rechargeButton_->setEnabled(true);
    logoutButton_->setEnabled(true);
    ordersButton_->setEnabled(true);
    preferenceButton_->setEnabled(true);
    stateLabel_->setObjectName(error ? QStringLiteral("statusError")
                                     : QStringLiteral("statusSuccess"));
    stateLabel_->style()->unpolish(stateLabel_);
    stateLabel_->style()->polish(stateLabel_);
    stateLabel_->setText(message);
    if (avatarRequest) clearPendingAvatar(error);
}

void UserProfileDialog::clearPendingAvatar(bool removeFile)
{
    if (removeFile && !pendingAvatarAbsolutePath_.isEmpty())
        QFile::remove(pendingAvatarAbsolutePath_);
    pendingAvatarAbsolutePath_.clear();
}

}
