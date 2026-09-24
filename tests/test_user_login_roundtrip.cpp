#include "network/network_server_host.h"
#include "service/user_client_facade.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>

#include <functional>

namespace {

bool waitUntil(const std::function<bool()> &condition)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < 4000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1);
    }
    return condition();
}

bool createPhase2Database(const QString &path)
{
    const QString connectionName = QStringLiteral("legacy_%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(path);
        if (database.open()) {
            QSqlQuery query(database);
            ok = query.exec(QStringLiteral(
                     "CREATE TABLE schema_version(version INTEGER PRIMARY KEY, applied_at TEXT NOT NULL)"))
                && query.exec(QStringLiteral(
                     "INSERT INTO schema_version(version, applied_at) VALUES(1, 'phase2')"));
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return ok;
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    const QString databasePath = directory.filePath(QStringLiteral("legacy-phase2.db"));
    if (!createPhase2Database(databasePath)) return 1;

    ncs::NetworkServerHost server(databasePath);
    quint16 port = 0;
    QObject::connect(&server, &ncs::NetworkServerHost::serverStarted,
                     [&](quint16 value) { port = value; });
    server.startServer(QStringLiteral("127.0.0.1"), 0);
    if (!waitUntil([&] { return port > 0; })) return 2;

    ncs::UserClientFacade facade;
    bool connected = false;
    QString displayCode;
    bool loggedIn = false;
    QString failure;
    QObject::connect(&facade, &ncs::UserClientFacade::connected,
                     [&] { connected = true; });
    QObject::connect(&facade, &ncs::UserClientFacade::otpReceived,
                     [&](const QString &code, int, int) { displayCode = code; });
    QObject::connect(&facade, &ncs::UserClientFacade::loginSucceeded,
                     [&](const ncs::User &user) {
                         loggedIn = user.phone == QStringLiteral("138****8004");
                     });
    QObject::connect(&facade, &ncs::UserClientFacade::requestFailed,
                     [&](const QString &, int, const QString &message) { failure = message; });

    facade.connectToServer(QStringLiteral("127.0.0.1"), port);
    if (!waitUntil([&] { return connected; })) return 3;
    facade.requestOtp(QStringLiteral("13800138004"));
    if (!waitUntil([&] { return !displayCode.isEmpty() || !failure.isEmpty(); })
        || displayCode.isEmpty()) return 4;
    facade.login(QStringLiteral("13800138004"), displayCode);
    if (!waitUntil([&] { return loggedIn || !failure.isEmpty(); }) || !loggedIn) return 5;

    facade.disconnectFromServer();
    server.shutdown();
    return 0;
}
