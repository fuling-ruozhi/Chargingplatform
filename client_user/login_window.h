#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;

namespace ncs {

struct User;
class UserClientFacade;

class LoginWindow : public QWidget
{
    Q_OBJECT
public:
    explicit LoginWindow(UserClientFacade &facade, QWidget *parent = nullptr);
    void resetForLogout();

signals:
    void loginCompleted(const ncs::User &user);

private:
    enum class AuthState {
        LoggedOut,
        OtpPending,
        OtpReady,
        LoginPending,
        LoggedIn,
        LogoutPending
    };

    void requestOtp();
    void submit();
    void beginRequest(const QString &route, const QString &message);
    void finishRequest(const QString &message, bool error);
    void updateCountdown();

    UserClientFacade &facade_;
    QLineEdit *phone_ = nullptr;
    QLineEdit *code_ = nullptr;
    QPushButton *otpButton_ = nullptr;
    QPushButton *loginButton_ = nullptr;
    QLabel *status_ = nullptr;
    QTimer *requestTimer_ = nullptr;
    QTimer *countdownTimer_ = nullptr;
    QString pendingRoute_;
    int cooldownRemaining_ = 0;
    AuthState authState_ = AuthState::LoggedOut;
};

}
