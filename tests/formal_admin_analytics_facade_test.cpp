#include "network/network_server_host.h"
#include "service/admin_client_facade.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QThread>

#include <functional>

namespace {

bool waitUntil(const std::function<bool()> &condition)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1);
    }
    return condition();
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::NetworkServerHost server(directory.filePath(QStringLiteral("facade.db")));
    ncs::AdminClientFacade facade;
    quint16 port = 0;
    bool connected = false;
    bool loggedIn = false;
    int summaries = 0;
    int trends = 0;
    int orders = 0;
    int statuses = 0;
    int chargers = 0;
    int chargerActions = 0;
    QObject::connect(&server, &ncs::NetworkServerHost::serverStarted,
                     [&](quint16 value) { port = value; });
    QObject::connect(&facade, &ncs::AdminClientFacade::connected,
                     [&] { connected = true; });
    QObject::connect(&facade, &ncs::AdminClientFacade::loginSucceeded,
                     [&](const ncs::Admin &) {
                         loggedIn = true;
                         facade.requestRevenueSummary();
                         facade.requestRevenueTrend(7);
                         facade.requestRecentOrders();
                         facade.requestChargerStatusSummary();
                         facade.requestChargers(QStringLiteral("NCS-01"), -1);
                     });
    QObject::connect(&facade, &ncs::AdminClientFacade::revenueSummaryReceived,
                     [&](const ncs::RevenueSummary &summary) {
                         if (summary.todayRevenue >= 0.0 && summary.monthRevenue >= 0.0
                             && summary.totalRevenue >= 0.0) ++summaries;
                     });
    QObject::connect(&facade, &ncs::AdminClientFacade::revenueTrendReceived,
                     [&](const ncs::RevenueTrend &trend) {
                         if (trend.days == 7 && trend.items.size() == 7) ++trends;
                     });
    QObject::connect(&facade, &ncs::AdminClientFacade::recentOrdersReceived,
                     [&](const ncs::RecentOrders &value) {
                         if (value.size() <= 10) ++orders;
                     });
    QObject::connect(&facade, &ncs::AdminClientFacade::chargerStatusReceived,
                     [&](const ncs::ChargerStatusSummary &summary) {
                         if (summary.total == 40
                             && summary.total == summary.idle + summary.inUse + summary.fault) {
                         ++statuses;
                     }
                 });
    QObject::connect(&facade, &ncs::AdminClientFacade::chargersReceived,
                     [&](const QVector<ncs::Charger> &value) {
                         if (!value.isEmpty()) {
                             ++chargers;
                             facade.restartCharger(value.first().id);
                         }
                     });
    QObject::connect(&facade, &ncs::AdminClientFacade::chargerActionSucceeded,
                     [&](const QString &, qint64) { ++chargerActions; });

    server.startServer(QStringLiteral("127.0.0.1"), 0);
    if (!waitUntil([&] { return port > 0; })) return 1;
    facade.connectToServer(QStringLiteral("127.0.0.1"), port);
    if (!waitUntil([&] { return connected; })) return 2;
    facade.login(QStringLiteral("admin"), QStringLiteral("123456"));
    if (!waitUntil([&] { return loggedIn && summaries == 1 && trends == 1
                              && orders == 1 && statuses == 1
                              && chargers == 1 && chargerActions == 1; })) return 3;
    facade.disconnectFromServer();
    server.shutdown();
    return 0;
}
