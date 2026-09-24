#include "network/network_server_host.h"
#include "service/admin_client_facade.h"
#include "service/user_client_facade.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSet>
#include <QTemporaryDir>
#include <QThread>

#include <functional>

namespace {

bool waitUntil(const std::function<bool()> &condition, int timeoutMs = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QThread::msleep(1);
    }
    return condition();
}

}

namespace ncs {

class AsyncLifecycleTestProbe
{
public:
    static QString token(const UserClientFacade &facade)
    {
        return facade.sessionToken_;
    }
    static QString token(const AdminClientFacade &facade)
    {
        return facade.sessionToken_;
    }
    static int pending(const UserClientFacade &facade)
    {
        return facade.requests_->pendingCount();
    }
    static int pending(const AdminClientFacade &facade)
    {
        return facade.requests_->pendingCount();
    }
};

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    ncs::NetworkServerHost server(
        directory.filePath(QStringLiteral("session-cycle.db")));
    quint16 port = 0;
    QObject::connect(&server, &ncs::NetworkServerHost::serverStarted,
                     [&](quint16 value) { port = value; });
    server.startServer(QStringLiteral("127.0.0.1"), 0);
    if (!waitUntil([&] { return port > 0; })) return 2;

    ncs::UserClientFacade userFacade;
    bool userConnected = false;
    int userLogins = 0;
    int userLogouts = 0;
    QString otp;
    QString userFailure;
    QObject::connect(&userFacade, &ncs::UserClientFacade::connected,
                     [&] { userConnected = true; });
    QObject::connect(&userFacade, &ncs::UserClientFacade::otpReceived,
                     [&](const QString &code, int, int) { otp = code; });
    QObject::connect(&userFacade, &ncs::UserClientFacade::loginSucceeded,
                     [&](const ncs::User &) { ++userLogins; });
    QObject::connect(&userFacade, &ncs::UserClientFacade::logoutSucceeded,
                     [&] { ++userLogouts; });
    QObject::connect(&userFacade, &ncs::UserClientFacade::requestFailed,
                     [&](const QString &, int, const QString &message) {
                         userFailure = message;
                     });
    userFacade.connectToServer(QStringLiteral("127.0.0.1"), port);
    if (!waitUntil([&] { return userConnected; })) return 3;

    QSet<QString> userTokens;
    for (int cycle = 0; cycle < 20; ++cycle) {
        otp.clear();
        const QString phone = QStringLiteral("1390000%1")
                                  .arg(cycle, 4, 10, QLatin1Char('0'));
        userFacade.requestOtp(phone);
        if (!waitUntil([&] { return !otp.isEmpty() || !userFailure.isEmpty(); })
            || otp.isEmpty())
            return 10 + cycle;
        const int expectedLogins = userLogins + 1;
        userFacade.login(phone, otp);
        if (!waitUntil([&] { return userLogins == expectedLogins; }))
            return 40 + cycle;
        const QString token = ncs::AsyncLifecycleTestProbe::token(userFacade);
        if (token.size() != 64 || userTokens.contains(token)) return 70 + cycle;
        userTokens.insert(token);
        if (ncs::AsyncLifecycleTestProbe::pending(userFacade) != 0)
            return 100 + cycle;
        const int expectedLogouts = userLogouts + 1;
        userFacade.logout();
        if (!waitUntil([&] { return userLogouts == expectedLogouts; }))
            return 130 + cycle;
        if (!ncs::AsyncLifecycleTestProbe::token(userFacade).isEmpty()
            || ncs::AsyncLifecycleTestProbe::pending(userFacade) != 0)
            return 160 + cycle;
    }
    userFacade.disconnectFromServer();
    if (!waitUntil([&] { return !userFacade.isConnected(); })) return 189;

    ncs::AdminClientFacade adminFacade;
    bool adminConnected = false;
    int adminLogins = 0;
    int adminLogouts = 0;
    QObject::connect(&adminFacade, &ncs::AdminClientFacade::connected,
                     [&] { adminConnected = true; });
    QObject::connect(&adminFacade, &ncs::AdminClientFacade::loginSucceeded,
                     [&](const ncs::Admin &) { ++adminLogins; });
    QObject::connect(&adminFacade, &ncs::AdminClientFacade::logoutSucceeded,
                     [&] { ++adminLogouts; });
    adminFacade.connectToServer(QStringLiteral("127.0.0.1"), port);
    if (!waitUntil([&] { return adminConnected; })) return 190;

    QSet<QString> adminTokens;
    for (int cycle = 0; cycle < 10; ++cycle) {
        const int expectedLogins = adminLogins + 1;
        adminFacade.login(QStringLiteral("admin"), QStringLiteral("123456"));
        if (!waitUntil([&] { return adminLogins == expectedLogins; }))
            return 200 + cycle;
        const QString token = ncs::AsyncLifecycleTestProbe::token(adminFacade);
        if (token.size() != 64 || adminTokens.contains(token)) return 220 + cycle;
        adminTokens.insert(token);
        if (ncs::AsyncLifecycleTestProbe::pending(adminFacade) != 0)
            return 240 + cycle;
        const int expectedLogouts = adminLogouts + 1;
        adminFacade.logout();
        if (!waitUntil([&] { return adminLogouts == expectedLogouts; }))
            return 260 + cycle;
        if (!ncs::AsyncLifecycleTestProbe::token(adminFacade).isEmpty()
            || ncs::AsyncLifecycleTestProbe::pending(adminFacade) != 0)
            return 280 + cycle;
    }

    adminFacade.disconnectFromServer();
    server.shutdown();
    return 0;
}
