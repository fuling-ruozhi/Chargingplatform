#include "login_window.h"
#include "network/network_server_host.h"
#include "service/user_client_facade.h"
#include "user_profile_dialog.h"

#include <QApplication>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>

#include <functional>

namespace {

bool waitUntil(const std::function<bool()> &condition)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < 5000) {
        QApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1);
    }
    return condition();
}

QPushButton *button(QWidget &widget, const QString &text)
{
    for (QPushButton *candidate : widget.findChildren<QPushButton *>()) {
        if (candidate->text() == text) return candidate;
    }
    return nullptr;
}

bool hasText(QWidget &widget, const QString &text)
{
    for (QLabel *label : widget.findChildren<QLabel *>()) {
        if (label->text().contains(text)) return true;
    }
    return false;
}

}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::NetworkServerHost server(directory.filePath(QStringLiteral("user-ui.db")));
    quint16 port = 0;
    QObject::connect(&server, &ncs::NetworkServerHost::serverStarted,
                     [&](quint16 value) { port = value; });
    server.startServer(QStringLiteral("127.0.0.1"), 0);
    if (!waitUntil([&] { return port > 0; })) return 1;

    ncs::UserClientFacade facade;
    bool connected = false;
    QString code;
    ncs::User user;
    QObject::connect(&facade, &ncs::UserClientFacade::connected,
                     [&] { connected = true; });
    QObject::connect(&facade, &ncs::UserClientFacade::otpReceived,
                     [&](const QString &value, int, int) { code = value; });
    QObject::connect(&facade, &ncs::UserClientFacade::loginSucceeded,
                     [&](const ncs::User &value) { user = value; });
    facade.connectToServer(QStringLiteral("127.0.0.1"), port);
    if (!waitUntil([&] { return connected; })) return 2;
    facade.requestOtp(QStringLiteral("13800138005"));
    if (!waitUntil([&] { return !code.isEmpty(); })) return 3;
    facade.login(QStringLiteral("13800138005"), code);
    if (!waitUntil([&] { return user.id > 0; })) return 4;

    ncs::LoginWindow login(facade);
    if (login.size() != QSize(420, 760)
        || !login.findChild<QLineEdit *>(QStringLiteral("phoneInput"))
        || !login.findChild<QLineEdit *>(QStringLiteral("codeInput"))
        || !button(login, QStringLiteral("获取验证码"))) return 5;

    ncs::UserProfileDialog profile(user, facade);
    profile.show();
    if (!waitUntil([&] { return hasText(profile, QStringLiteral("138****8005"))
                                 && hasText(profile, QStringLiteral("资料已更新")); }))
        return 6;
    if (!button(profile, QStringLiteral("我的订单"))) return 11;
    auto *nickname = profile.findChild<QLineEdit *>(QStringLiteral("nicknameInput"));
    if (!nickname || !button(profile, QStringLiteral("保存昵称"))) return 7;
    nickname->setText(QStringLiteral("界面用户"));
    button(profile, QStringLiteral("保存昵称"))->click();
    if (!waitUntil([&] { return nickname->text() == QStringLiteral("界面用户")
                                 && hasText(profile, QStringLiteral("保存成功")); })) return 8;

    auto *amount = profile.findChild<QDoubleSpinBox *>(QStringLiteral("rechargeAmount"));
    if (!amount || !button(profile, QStringLiteral("确认充值"))) return 9;
    amount->setValue(8.88);
    button(profile, QStringLiteral("确认充值"))->click();
    auto *confirmRecharge = profile.findChild<QPushButton *>(
        QStringLiteral("confirmSubmitButton"));
    if (!confirmRecharge) return 12;
    confirmRecharge->click();
    if (!waitUntil([&] { return hasText(profile, QStringLiteral("¥8.88")); })) return 10;

    profile.close();
    facade.disconnectFromServer();
    server.shutdown();
    return 0;
}
