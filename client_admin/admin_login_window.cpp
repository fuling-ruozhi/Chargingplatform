#include "admin_login_window.h"

#include "model/business_error.h"
#include "service/admin_client_facade.h"

#include <QFile>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace ncs {

AdminLoginWindow::AdminLoginWindow(AdminClientFacade &facade, QWidget *parent)
    : QWidget(parent), facade_(facade), requestTimer_(new QTimer(this)),
      lockTimer_(new QTimer(this))
{
    setWindowTitle(QStringLiteral("NCS 管理端登录"));
    setFixedSize(920, 560);
    // 本窗口（20fcb6f/e58f22e 重设计）的 objectName 配套 theme.qss；全局已由
    // main.cpp 加载供主窗口/页面使用的 admin.qss，这里在窗口子树内改用
    // theme.qss 覆盖（窗口级样式表优先于应用级）。
    QFile theme(QStringLiteral(":/client_admin/theme.qss"));
    if (theme.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStyleSheet(QString::fromUtf8(theme.readAll()));
    }
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(34, 34, 34, 34);
    layout->setSpacing(18);

    auto *brandPanel = new QFrame(this);
    brandPanel->setObjectName(QStringLiteral("adminBrandPanel"));
    brandPanel->setFixedWidth(410);
    auto *brandLayout = new QVBoxLayout(brandPanel);
    brandLayout->setContentsMargins(38, 42, 38, 38);
    brandLayout->setSpacing(12);
    auto *eyebrow = new QLabel(QStringLiteral("NCS  CHARGING CLOUD"), brandPanel);
    eyebrow->setObjectName(QStringLiteral("adminHeroEyebrow"));
    auto *brand = new QLabel(QStringLiteral("让充电运营\n更清晰、更高效"), brandPanel);
    brand->setObjectName(QStringLiteral("adminHeroTitle"));
    auto *subtitle = new QLabel(
        QStringLiteral("统一查看站点、电桩、订单与运营状态，\n为每一次可靠服务提供数据支撑。"),
        brandPanel);
    subtitle->setObjectName(QStringLiteral("adminHeroSubtitle"));
    subtitle->setWordWrap(true);
    auto *scope = new QLabel(
        QStringLiteral("站点管理  ·  电桩监控  ·  订单分析"), brandPanel);
    scope->setObjectName(QStringLiteral("adminHeroPill"));
    brandLayout->addWidget(eyebrow);
    brandLayout->addSpacing(22);
    brandLayout->addWidget(brand);
    brandLayout->addWidget(subtitle);
    brandLayout->addStretch();
    brandLayout->addWidget(scope);

    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("loginCard"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(36, 38, 36, 34);
    cardLayout->setSpacing(12);
    auto *loginTitle = new QLabel(QStringLiteral("欢迎回来"), card);
    loginTitle->setObjectName(QStringLiteral("pageTitle"));
    auto *loginSubtitle = new QLabel(
        QStringLiteral("使用管理员账号进入 NCS 运营中心"), card);
    loginSubtitle->setObjectName(QStringLiteral("mutedLabel"));
    username_ = new QLineEdit(QStringLiteral("admin"), card);
    username_->setObjectName(QStringLiteral("adminUsernameInput"));
    username_->setPlaceholderText(QStringLiteral("管理员账号"));
    password_ = new QLineEdit(card);
    password_->setObjectName(QStringLiteral("adminPasswordInput"));
    password_->setPlaceholderText(QStringLiteral("管理员密码"));
    password_->setEchoMode(QLineEdit::Password);
    password_->setMaxLength(128);
    loginButton_ = new QPushButton(QStringLiteral("登录管理后台"), card);
    loginButton_->setObjectName(QStringLiteral("adminLoginButton"));
    loginButton_->setDefault(true);
    loginButton_->setEnabled(false);
    status_ = new QLabel(QStringLiteral("正在启动本地服务…"), card);
    status_->setObjectName(QStringLiteral("statusLoading"));
    status_->setAlignment(Qt::AlignCenter);
    status_->setWordWrap(true);
    cardLayout->addWidget(loginTitle);
    cardLayout->addWidget(loginSubtitle);
    cardLayout->addSpacing(18);
    cardLayout->addWidget(username_);
    cardLayout->addWidget(password_);
    cardLayout->addWidget(loginButton_);
    cardLayout->addWidget(status_);
    cardLayout->addStretch();
    layout->addWidget(brandPanel);
    layout->addWidget(card, 1);

    requestTimer_->setSingleShot(true);
    requestTimer_->setInterval(10000);
    lockTimer_->setInterval(1000);
    connect(loginButton_, &QPushButton::clicked, this, &AdminLoginWindow::submit);
    connect(password_, &QLineEdit::returnPressed, loginButton_, &QPushButton::click);
    connect(requestTimer_, &QTimer::timeout, this, [this] {
        finish(QStringLiteral("登录请求超时，请检查服务状态"), true);
    });
    connect(lockTimer_, &QTimer::timeout, this, &AdminLoginWindow::updateLock);
    connect(&facade_, &AdminClientFacade::connected, this, [this] {
        finish(QStringLiteral("服务已就绪，请登录"), false);
    });
    connect(&facade_, &AdminClientFacade::disconnected, this, [this] {
        serverReady_ = false;
        finish(QStringLiteral("本地服务连接已断开"), true);
    });
    connect(&facade_, &AdminClientFacade::networkError, this,
            [this](const QString &) {
                finish(QStringLiteral("无法连接本地服务，请稍后重试"), true);
            });
    connect(&facade_, &AdminClientFacade::requestFailed, this,
            [this](const QString &route, int code, const QString &message, int retry) {
                if (route != QStringLiteral("admin.login")) return;
                if (code == static_cast<int>(BusinessErrorCode::AdminLocked)) {
                    beginLock(retry > 0 ? retry : 30);
                } else {
                    finish(message.isEmpty() ? QStringLiteral("账号或密码错误") : message,
                           true);
                }
            });
    connect(&facade_, &AdminClientFacade::loginSucceeded, this,
            [this](const Admin &admin) {
                requestTimer_->stop();
                requestPending_ = false;
                loginButton_->setEnabled(true);
                password_->clear();
                emit loginCompleted(admin);
            });
}

void AdminLoginWindow::setServerReady(bool ready)
{
    serverReady_ = ready;
    loginButton_->setEnabled(ready && facade_.isConnected()
                             && lockRemaining_ == 0);
}

void AdminLoginWindow::setServerError(const QString &message)
{
    serverReady_ = false;
    loginButton_->setEnabled(false);
    finish(QStringLiteral("服务启动失败：%1").arg(message), true);
}

void AdminLoginWindow::resetForLogout()
{
    requestPending_ = false;
    requestTimer_->stop();
    password_->clear();
    finish(QStringLiteral("已安全退出，请重新登录"), false);
}

void AdminLoginWindow::submit()
{
    if (requestPending_) return;
    if (username_->text().trimmed().isEmpty() || password_->text().isEmpty()) {
        finish(QStringLiteral("请输入管理员账号和密码"), true);
        return;
    }
    if (!facade_.isConnected()) {
        finish(QStringLiteral("本地服务尚未连接，请稍后重试"), true);
        return;
    }
    loginButton_->setEnabled(false);
    requestPending_ = true;
    status_->setObjectName(QStringLiteral("statusLoading"));
    status_->setText(QStringLiteral("正在验证管理员身份…"));
    status_->style()->unpolish(status_);
    status_->style()->polish(status_);
    requestTimer_->start();
    const QString password = password_->text();
    password_->clear();
    facade_.login(username_->text(), password);
}

void AdminLoginWindow::finish(const QString &message, bool error)
{
    requestTimer_->stop();
    requestPending_ = false;
    if (lockRemaining_ == 0) {
        loginButton_->setEnabled(serverReady_ && facade_.isConnected());
    }
    status_->setObjectName(error ? QStringLiteral("statusError")
                                 : QStringLiteral("statusSuccess"));
    status_->setText(message);
    status_->style()->unpolish(status_);
    status_->style()->polish(status_);
}

void AdminLoginWindow::beginLock(int seconds)
{
    requestTimer_->stop();
    lockRemaining_ = qMax(1, seconds);
    loginButton_->setEnabled(false);
    username_->setEnabled(false);
    password_->setEnabled(false);
    status_->setObjectName(QStringLiteral("statusError"));
    status_->setText(QStringLiteral("连续失败过多，%1 秒后可重试").arg(lockRemaining_));
    status_->style()->unpolish(status_);
    status_->style()->polish(status_);
    lockTimer_->start();
}

void AdminLoginWindow::updateLock()
{
    --lockRemaining_;
    if (lockRemaining_ <= 0) {
        lockTimer_->stop();
        username_->setEnabled(true);
        password_->setEnabled(true);
        loginButton_->setEnabled(serverReady_ && facade_.isConnected());
        finish(QStringLiteral("锁定已解除，可以重新登录"), false);
        return;
    }
    status_->setObjectName(QStringLiteral("statusError"));
    status_->setText(QStringLiteral("连续失败过多，%1 秒后可重试").arg(lockRemaining_));
    status_->style()->unpolish(status_);
    status_->style()->polish(status_);
}

}
