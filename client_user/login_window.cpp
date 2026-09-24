#include "login_window.h"

#include "service/user_client_facade.h"
#include "user_messages.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace ncs {

LoginWindow::LoginWindow(UserClientFacade &facade, QWidget *parent)
    : QWidget(parent), facade_(facade), phone_(new QLineEdit(this)),
      code_(new QLineEdit(this)),
      otpButton_(new QPushButton(QStringLiteral("获取验证码"), this)),
      loginButton_(new QPushButton(QStringLiteral("登录 / 自动注册"), this)),
      status_(new QLabel(QStringLiteral("正在连接服务…"), this)),
      requestTimer_(new QTimer(this)), countdownTimer_(new QTimer(this))
{
    setWindowTitle(QStringLiteral("NCS 用户登录"));
    setFixedSize(420, 760);
    setObjectName(QStringLiteral("loginRoot"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 24, 20, 24);
    layout->setSpacing(18);

    auto *hero = new QFrame(this);
    hero->setObjectName(QStringLiteral("loginHero"));
    hero->setFixedHeight(210);
    auto *heroLayout = new QVBoxLayout(hero);
    heroLayout->setContentsMargins(24, 28, 24, 26);
    heroLayout->setSpacing(8);
    auto *brand = new QLabel(QStringLiteral("NCS"), hero);
    brand->setObjectName(QStringLiteral("heroTitle"));
    auto *titleLine = new QLabel(QStringLiteral("新能源智慧充电"), hero);
    titleLine->setObjectName(QStringLiteral("heroTitle"));
    auto *subtitle = new QLabel(
        QStringLiteral("快速发现附近电站，安心完成每一次充电"), hero);
    subtitle->setObjectName(QStringLiteral("heroSubtitle"));
    subtitle->setWordWrap(true);
    heroLayout->addWidget(brand);
    heroLayout->addStretch();
    heroLayout->addWidget(titleLine);
    heroLayout->addWidget(subtitle);
    layout->addWidget(hero);

    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("loginCard"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(22, 22, 22, 22);
    cardLayout->setSpacing(11);
    auto *title = new QLabel(QStringLiteral("欢迎使用 NCS"), card);
    title->setObjectName(QStringLiteral("sectionTitle"));
    auto *hint = new QLabel(QStringLiteral("手机号验证登录，首次使用将自动创建账号"), card);
    hint->setObjectName(QStringLiteral("mutedLabel"));

    phone_->setObjectName(QStringLiteral("phoneInput"));
    phone_->setPlaceholderText(QStringLiteral("请输入11位手机号"));
    phone_->setMaxLength(11);
    phone_->setClearButtonEnabled(true);
    phone_->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[0-9]{0,11}")), phone_));
    code_->setObjectName(QStringLiteral("codeInput"));
    code_->setPlaceholderText(QStringLiteral("请输入6位验证码"));
    code_->setMaxLength(6);
    code_->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[0-9]{0,6}")), code_));

    auto *codeRow = new QHBoxLayout;
    codeRow->setSpacing(8);
    codeRow->addWidget(code_, 1);
    otpButton_->setObjectName(QStringLiteral("secondaryButton"));
    otpButton_->setFixedWidth(120);
    codeRow->addWidget(otpButton_);
    loginButton_->setDefault(true);
    status_->setObjectName(QStringLiteral("statusLoading"));
    status_->setAlignment(Qt::AlignCenter);
    status_->setWordWrap(true);

    cardLayout->addWidget(title);
    cardLayout->addWidget(hint);
    cardLayout->addSpacing(6);
    cardLayout->addWidget(phone_);
    cardLayout->addLayout(codeRow);
    cardLayout->addWidget(loginButton_);
    cardLayout->addWidget(status_);
    layout->addWidget(card);
    layout->addStretch();

    requestTimer_->setSingleShot(true);
    requestTimer_->setInterval(10000);
    countdownTimer_->setInterval(1000);
    connect(otpButton_, &QPushButton::clicked, this, &LoginWindow::requestOtp);
    connect(loginButton_, &QPushButton::clicked, this, &LoginWindow::submit);
    connect(code_, &QLineEdit::returnPressed, loginButton_, &QPushButton::click);
    connect(countdownTimer_, &QTimer::timeout, this, &LoginWindow::updateCountdown);
    connect(requestTimer_, &QTimer::timeout, this, [this] {
        if (!pendingRoute_.isEmpty()) {
            finishRequest(QStringLiteral("请求超时，请确认服务端正常运行后重试"), true);
        }
    });
    connect(&facade_, &UserClientFacade::connected, this, [this] {
        if (pendingRoute_.isEmpty()) finishRequest(QStringLiteral("服务已连接"), false);
    });
    connect(&facade_, &UserClientFacade::disconnected, this, [this] {
        authState_ = AuthState::LoggedOut;
        finishRequest(QStringLiteral("服务连接已断开，请确认管理端正在运行"), true);
    });
    connect(&facade_, &UserClientFacade::networkError, this,
            [this](const QString &message) { finishRequest(userFacingError(message), true); });
    connect(&facade_, &UserClientFacade::requestFailed, this,
            [this](const QString &route, int, const QString &message) {
                if (route == pendingRoute_) finishRequest(userFacingError(message), true);
            });
    connect(&facade_, &UserClientFacade::otpReceived, this,
            [this](const QString &displayCode, int cooldown, int) {
                cooldownRemaining_ = cooldown;
                authState_ = AuthState::OtpReady;
                countdownTimer_->start();
                pendingRoute_.clear();
                requestTimer_->stop();
                loginButton_->setEnabled(true);
                status_->setObjectName(QStringLiteral("statusSuccess"));
                status_->style()->unpolish(status_);
                status_->style()->polish(status_);
                status_->setText(displayCode.isEmpty()
                    ? QStringLiteral("验证码已发送，5分钟内有效")
                    : QStringLiteral("模拟短信验证码：%1（5分钟内有效）")
                          .arg(displayCode));
                updateCountdown();
            });
    connect(&facade_, &UserClientFacade::loginSucceeded, this,
            [this](const User &user) {
                requestTimer_->stop();
                pendingRoute_.clear();
                authState_ = AuthState::LoggedIn;
                emit loginCompleted(user);
            });
}

void LoginWindow::resetForLogout()
{
    authState_ = AuthState::LoggedOut;
    requestTimer_->stop();
    countdownTimer_->stop();
    pendingRoute_.clear();
    cooldownRemaining_ = 0;
    code_->clear();
    code_->setEnabled(true);
    phone_->setEnabled(true);
    loginButton_->setEnabled(true);
    otpButton_->setEnabled(true);
    otpButton_->setText(QStringLiteral("获取验证码"));
    status_->setObjectName(QStringLiteral("statusSuccess"));
    status_->setText(QStringLiteral("已安全退出，请重新获取验证码登录"));
    status_->style()->unpolish(status_);
    status_->style()->polish(status_);
}

void LoginWindow::requestOtp()
{
    if (!pendingRoute_.isEmpty()
        || authState_ == AuthState::OtpPending
        || authState_ == AuthState::LoginPending) return;
    const QString phone = phone_->text().trimmed();
    if (!QRegularExpression(QStringLiteral("^1[0-9]{10}$")).match(phone).hasMatch()) {
        finishRequest(QStringLiteral("请输入11位、1开头的手机号"), true);
        return;
    }
    if (!facade_.isConnected()) {
        finishRequest(QStringLiteral("服务尚未连接，请确认管理端已启动"), true);
        facade_.connectToServer();
        return;
    }
    beginRequest(QStringLiteral("user.otp.request"), QStringLiteral("正在发送验证码…"));
    facade_.requestOtp(phone);
}

void LoginWindow::submit()
{
    if (!pendingRoute_.isEmpty()
        || authState_ == AuthState::OtpPending
        || authState_ == AuthState::LoginPending) return;
    if (phone_->text().size() != 11 || code_->text().size() != 6) {
        finishRequest(QStringLiteral("请输入手机号和6位验证码"), true);
        return;
    }
    if (!facade_.isConnected()) {
        finishRequest(QStringLiteral("服务尚未连接，请确认管理端已启动"), true);
        facade_.connectToServer();
        return;
    }
    beginRequest(QStringLiteral("user.login"), QStringLiteral("正在验证并登录…"));
    facade_.login(phone_->text(), code_->text());
}

void LoginWindow::beginRequest(const QString &route, const QString &message)
{
    pendingRoute_ = route;
    authState_ = route == QStringLiteral("user.otp.request")
        ? AuthState::OtpPending : AuthState::LoginPending;
    loginButton_->setEnabled(false);
    if (route == QStringLiteral("user.otp.request")) otpButton_->setEnabled(false);
    status_->setObjectName(QStringLiteral("statusLoading"));
    status_->style()->unpolish(status_);
    status_->style()->polish(status_);
    status_->setText(message);
    requestTimer_->start();
}

void LoginWindow::finishRequest(const QString &message, bool error)
{
    const QString finishedRoute = pendingRoute_;
    requestTimer_->stop();
    pendingRoute_.clear();
    if (authState_ != AuthState::LoggedIn) {
        authState_ = finishedRoute == QStringLiteral("user.login")
            && cooldownRemaining_ > 0
            ? AuthState::OtpReady : AuthState::LoggedOut;
    }
    loginButton_->setEnabled(true);
    if (cooldownRemaining_ == 0) otpButton_->setEnabled(true);
    status_->setObjectName(error ? QStringLiteral("statusError")
                                 : QStringLiteral("statusSuccess"));
    status_->style()->unpolish(status_);
    status_->style()->polish(status_);
    status_->setText(message);
}

void LoginWindow::updateCountdown()
{
    if (cooldownRemaining_ <= 0) {
        countdownTimer_->stop();
        otpButton_->setEnabled(true);
        otpButton_->setText(QStringLiteral("重新获取"));
        return;
    }
    otpButton_->setEnabled(false);
    otpButton_->setText(QStringLiteral("%1 秒后重试").arg(cooldownRemaining_));
    --cooldownRemaining_;
}

}
