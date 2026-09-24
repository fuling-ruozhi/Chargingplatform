#include "charging_dialog.h"
#include "database/database_manager.h"
#include "network/network_server_host.h"
#include "repository/charge_repository.h"
#include "repository/user_repository.h"
#include "service/charge_service.h"
#include "service/user_client_facade.h"
#include "service/user_service.h"
#include "user_main_window.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QPushButton>
#include <QSqlQuery>
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

bool hasText(QWidget &widget, const QString &text)
{
    for (QLabel *label : widget.findChildren<QLabel *>()) {
        if (label->text().contains(text)) return true;
    }
    return false;
}

bool hasButton(QWidget &widget, const QString &text)
{
    for (QPushButton *button : widget.findChildren<QPushButton *>()) {
        if (button->text() == text) return true;
    }
    return false;
}

}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QTemporaryDir directory;
    const QString databasePath = directory.filePath(QStringLiteral("recovery-ui.db"));
    const QString phone = QStringLiteral("13800138402");
    {
        ncs::DatabaseManager database(databasePath);
        if (!database.initialize()) return 1;
        ncs::UserRepository users(database);
        ncs::UserService userService(database, users);
        const auto otp = userService.requestOtp(phone);
        const auto user = userService.loginWithOtp(phone, otp.value.displayCode);
        if (!otp.success || !user.success
            || !userService.recharge(user.value.id, 100.0).success) return 2;
        QSqlQuery charger(database.connection());
        if (!charger.exec(QStringLiteral(
                "SELECT id FROM charger WHERE status=0 ORDER BY id LIMIT 1"))
            || !charger.next()) return 3;
        ncs::ChargeRepository orders(database);
        ncs::ChargeService charges(database, orders);
        const auto reserved = charges.reserve(user.value.id,
                                               charger.value(0).toLongLong());
        const auto started = charges.startReserved(user.value.id, reserved.value.id);
        if (!reserved.success || !started.success) return 4;
    }

    ncs::NetworkServerHost server(databasePath);
    quint16 port = 0;
    QObject::connect(&server, &ncs::NetworkServerHost::serverStarted,
                     [&](quint16 value) { port = value; });
    server.startServer(QStringLiteral("127.0.0.1"), 0);
    if (!waitUntil([&] { return port > 0; })) return 5;

    ncs::UserClientFacade facade;
    QString code;
    ncs::User loggedIn;
    QObject::connect(&facade, &ncs::UserClientFacade::otpReceived,
                     [&](const QString &value, int, int) { code = value; });
    QObject::connect(&facade, &ncs::UserClientFacade::loginSucceeded,
                     [&](const ncs::User &user) { loggedIn = user; });
    facade.connectToServer(QStringLiteral("127.0.0.1"), port);
    if (!waitUntil([&] { return facade.isConnected(); })) return 6;
    facade.requestOtp(phone);
    if (!waitUntil([&] { return !code.isEmpty(); })) return 7;
    facade.login(phone, code);
    if (!waitUntil([&] { return loggedIn.id > 0; })) return 8;

    ncs::UserMainWindow home(loggedIn, facade);
    home.show();
    ncs::ChargingDialog *recovered = nullptr;
    if (!waitUntil([&] {
            recovered = home.findChild<ncs::ChargingDialog *>(
                QStringLiteral("recoveredChargingDialog"));
            return recovered && recovered->isVisible();
        })) return 9;
    if (!hasText(*recovered, QStringLiteral("正在充电"))
        || !hasButton(*recovered, QStringLiteral("结束充电并结算"))) return 10;

    recovered->close();
    home.close();
    facade.disconnectFromServer();
    server.shutdown();
    return 0;
}
