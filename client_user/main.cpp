#include "login_window.h"
#include "service/user_client_facade.h"
#include "user_main_window.h"
#include "util/logger.h"
#include <QApplication>
#include <QFile>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    ncs::Logger::info(QStringLiteral("user-app"), QStringLiteral("Application started"));
    QFile styleFile(QStringLiteral(":/client_user/theme.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }

    ncs::UserClientFacade facade;
    ncs::LoginWindow login(facade);
    ncs::UserMainWindow *home = nullptr;
    QObject::connect(&login, &ncs::LoginWindow::loginCompleted, &app,
                     [&](const ncs::User &user) {
                         delete home;
                         home = new ncs::UserMainWindow(user, facade);
                         QObject::connect(home, &ncs::UserMainWindow::logoutRequested,
                                          &app, [&] {
                                              home->deleteLater();
                                              home = nullptr;
                                              login.resetForLogout();
                                              login.show();
                                              login.raise();
                                              login.activateWindow();
                                          });
                         home->show();
                         login.hide();
                     });
    facade.connectToServer();
    login.show();
    const int result = app.exec();
    delete home;
    facade.disconnectFromServer();
    ncs::Logger::info(QStringLiteral("user-app"), QStringLiteral("Application stopped"));
    return result;
}
