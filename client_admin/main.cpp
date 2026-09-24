#include "admin_login_window.h"
#include "admin_main_window.h"
#include "network/network_server_host.h"
#include "service/admin_client_facade.h"
#include "util/logger.h"

#include <QApplication>
#include <QFile>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    ncs::Logger::info(QStringLiteral("admin-app"), QStringLiteral("Application started"));
    // 暂时加载与当前 admin 窗口/页面对象名配套的 admin.qss。
    // 20fcb6f 切换到 theme.qss 时未并入配套的窗口/页面重写（adminNavigation、
    // dashboardHero 等选择器在现有代码中不存在），导致登录后布局样式污染；
    // 待配套重写并入 master 后再切回 theme.qss。
    QFile style(QStringLiteral(":/client_admin/admin.qss"));
    if (style.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(style.readAll()));
    }

    ncs::NetworkServerHost server;
    ncs::AdminClientFacade facade;
    ncs::AdminLoginWindow login(facade);
    ncs::AdminMainWindow *mainWindow = nullptr;

    QObject::connect(&server, &ncs::NetworkServerHost::serverStarted,
                     &app, [&](quint16 port) {
                         facade.connectToServer(QStringLiteral("127.0.0.1"), port);
                         login.setServerReady(true);
                         if (mainWindow) {
                             mainWindow->setServerStatus(true, port);
                         }
                     });
    QObject::connect(&server, &ncs::NetworkServerHost::serverStopped,
                     &app, [&] {
                         if (mainWindow) {
                             mainWindow->setServerStatus(false);
                         }
                     });
    QObject::connect(&facade, &ncs::AdminClientFacade::disconnected,
                     &app, [&] {
                         if (mainWindow) {
                             mainWindow->setServerStatus(false);
                         }
                     });
    QObject::connect(&server, &ncs::NetworkServerHost::databaseError,
                     &app, [&](const QString &message) {
                         login.setServerError(message);
                     });
    QObject::connect(&server, &ncs::NetworkServerHost::serverError,
                     &app, [&](const QString &message) {
                         login.setServerError(message);
                     });
    QObject::connect(&login, &ncs::AdminLoginWindow::loginCompleted,
                     &app, [&](const ncs::Admin &admin) {
                         delete mainWindow;
                         mainWindow = new ncs::AdminMainWindow(admin, facade);
                         QObject::connect(mainWindow,
                             &ncs::AdminMainWindow::logoutRequested,
                             &facade, &ncs::AdminClientFacade::logout);
                         mainWindow->show();
                         login.hide();
                     });
    QObject::connect(&facade, &ncs::AdminClientFacade::logoutSucceeded,
                     &app, [&] {
                         if (mainWindow) mainWindow->deleteLater();
                         mainWindow = nullptr;
                         login.resetForLogout();
                         login.show();
                         login.raise();
                         login.activateWindow();
                     });

    server.startServer();
    login.show();
    const int result = app.exec();
    delete mainWindow;
    facade.disconnectFromServer();
    server.shutdown();
    ncs::Logger::info(QStringLiteral("admin-app"), QStringLiteral("Application stopped"));
    return result;
}
