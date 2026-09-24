#pragma once

#include "model/user.h"

#include <QCameraDevice>
#include <QDialog>
#include <QList>
#include <QPointer>

#include <functional>

class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;

namespace ncs {

class UserClientFacade;
class ConfirmActionDialog;
class AvatarSourceDialog;
class CameraUnavailableDialog;
class CameraCaptureDialog;

class UserProfileDialog : public QDialog
{
    Q_OBJECT
public:
    using CameraDevicesProvider = std::function<QList<QCameraDevice>()>;

    UserProfileDialog(const User &user, UserClientFacade &facade,
                      QWidget *parent = nullptr,
                      CameraDevicesProvider cameraDevices = {});
    ~UserProfileDialog() override;

protected:
    void reject() override;

signals:
    void userChanged(const ncs::User &user);
    void logoutRequested();
    void preferenceRequested();

private slots:
    void applyAvatarFromFile(const QString &sourcePath);

private:
    void render();
    void chooseAvatar();
    void chooseAvatarFromAlbum();
    void chooseAvatarFromCamera();
    void showNoCameraFallback();
    void updateNickname();
    void recharge();
    void openOrders();
    void beginRequest(const QString &route, const QString &message);
    void finishRequest(const QString &message, bool error);
    void clearPendingAvatar(bool removeFile);

    User user_;
    UserClientFacade &facade_;
    CameraDevicesProvider cameraDevices_;
    QLabel *avatarLabel_ = nullptr;
    QLabel *phoneLabel_ = nullptr;
    QLabel *balanceLabel_ = nullptr;
    QLabel *createdLabel_ = nullptr;
    QLabel *stateLabel_ = nullptr;
    QLineEdit *nicknameEdit_ = nullptr;
    QDoubleSpinBox *amountEdit_ = nullptr;
    QPushButton *nicknameButton_ = nullptr;
    QPushButton *avatarButton_ = nullptr;
    QPushButton *rechargeButton_ = nullptr;
    QPushButton *logoutButton_ = nullptr;
    QPushButton *ordersButton_ = nullptr;
    QPushButton *preferenceButton_ = nullptr;
    QTimer *requestTimer_ = nullptr;
    QString pendingRoute_;
    QString pendingAvatarAbsolutePath_;
    QPointer<ConfirmActionDialog> confirmation_;
    QPointer<AvatarSourceDialog> avatarSourceDialog_;
    QPointer<CameraUnavailableDialog> cameraUnavailableDialog_;
    QPointer<CameraCaptureDialog> cameraDialog_;
};

}
