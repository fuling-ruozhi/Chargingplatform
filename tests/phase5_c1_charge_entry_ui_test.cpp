#include "database/database_manager.h"
#include "network/network_server_host.h"
#include "repository/charge_repository.h"
#include "repository/station_repository.h"
#include "repository/user_repository.h"
#include "service/charge_service.h"
#include "service/station_service.h"
#include "service/user_client_facade.h"
#include "service/user_service.h"
#include "station_detail_dialog.h"

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

bool hasLabel(QWidget &widget, const QString &text)
{
    for (QLabel *label : widget.findChildren<QLabel *>()) {
        if (label->text() == text) return true;
    }
    return false;
}

QPushButton *button(QWidget &widget, const QString &text)
{
    for (QPushButton *candidate : widget.findChildren<QPushButton *>()) {
        if (candidate->text() == text) return candidate;
    }
    return nullptr;
}

}  // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QTemporaryDir directory;
    const QString databasePath = directory.filePath(QStringLiteral("charge-entry-ui.db"));

    ncs::DatabaseManager database(databasePath);
    if (!database.initialize()) return 1;
    QSqlQuery setup(database.connection());
    if (!setup.exec(QStringLiteral("UPDATE charger SET status=0 WHERE id IN (1,2)"))
        || !setup.exec(QStringLiteral("UPDATE charger SET status=2 WHERE id=3"))) return 2;

    ncs::UserRepository userRepository(database);
    ncs::UserService userService(database, userRepository);
    const QString phoneA = QStringLiteral("13800138501");
    const auto otpA = userService.requestOtp(phoneA);
    const auto userA = userService.loginWithOtp(phoneA, otpA.value.displayCode);
    const QString phoneB = QStringLiteral("13800138502");
    const auto otpB = userService.requestOtp(phoneB);
    const auto userB = userService.loginWithOtp(phoneB, otpB.value.displayCode);
    if (!userA.success || !userB.success) return 3;
    if (!userService.recharge(userA.value.id, 10.0).success
        || !userService.recharge(userB.value.id, 10.0).success) return 19;

    ncs::ChargeRepository chargeRepository(database);
    ncs::ChargeService chargeService(database, chargeRepository);
    const auto reserved = chargeService.reserve(userA.value.id, 1);
    if (!reserved.success) return 4;
    ncs::StationRepository stationRepository(database);
    ncs::StationService stationService(stationRepository);
    const auto detailResult = stationService.detail(1);
    if (!detailResult.success) return 5;

    ncs::NetworkServerHost server(databasePath);
    quint16 port = 0;
    QObject::connect(&server, &ncs::NetworkServerHost::serverStarted,
                     [&](quint16 value) { port = value; });
    server.startServer(QStringLiteral("127.0.0.1"), 0);
    if (!waitUntil([&] { return port > 0; })) return 6;

    ncs::UserClientFacade facade;
    bool connected = false;
    QObject::connect(&facade, &ncs::UserClientFacade::connected,
                     [&] { connected = true; });
    facade.connectToServer(QStringLiteral("127.0.0.1"), port);
    if (!waitUntil([&] { return connected; })) return 7;

    QString displayCode;
    bool loggedIn = false;
    QObject::connect(&facade, &ncs::UserClientFacade::otpReceived,
                     [&](const QString &code, int, int) { displayCode = code; });
    QObject::connect(&facade, &ncs::UserClientFacade::loginSucceeded,
                     [&](const ncs::User &user) { loggedIn = user.id == userA.value.id; });
    facade.requestOtp(phoneA);
    if (!waitUntil([&] { return !displayCode.isEmpty(); })) return 14;
    facade.login(phoneA, displayCode);
    if (!waitUntil([&] { return loggedIn; })) return 15;

    ncs::UserClientFacade facadeB;
    bool connectedB = false;
    QString displayCodeB;
    bool loggedInB = false;
    QObject::connect(&facadeB, &ncs::UserClientFacade::connected,
                     [&] { connectedB = true; });
    QObject::connect(&facadeB, &ncs::UserClientFacade::otpReceived,
                     [&](const QString &code, int, int) { displayCodeB = code; });
    QObject::connect(&facadeB, &ncs::UserClientFacade::loginSucceeded,
                     [&](const ncs::User &user) { loggedInB = user.id == userB.value.id; });
    facadeB.connectToServer(QStringLiteral("127.0.0.1"), port);
    if (!waitUntil([&] { return connectedB; })) return 16;
    facadeB.requestOtp(phoneB);
    if (!waitUntil([&] { return !displayCodeB.isEmpty(); })) return 17;
    facadeB.login(phoneB, displayCodeB);
    if (!waitUntil([&] { return loggedInB; })) return 18;

    {
        ncs::StationDetailDialog own(detailResult.value, userA.value.id, facade);
        own.show();
        if (!waitUntil([&] {
                return hasLabel(own, QStringLiteral("我的预约"))
                    && button(own, QStringLiteral("开始充电"))
                    && button(own, QStringLiteral("取消预约"))
                    && button(own, QStringLiteral("预约"))
                    && button(own, QStringLiteral("智慧方案"))
                    && !button(own, QStringLiteral("充电"));
            })) return 8;
        own.close();
    }

    bool stillReserved = false;
    qint64 activeId = 0;
    QObject::connect(&facade, &ncs::UserClientFacade::activeChargeReceived,
                     [&](bool active, const ncs::ChargingRecord &record) {
                         stillReserved = active
                             && record.status == ncs::ChargingOrderStatus::Reserved;
                         activeId = record.id;
                     });
    facade.requestActiveCharge(userA.value.id);
    if (!waitUntil([&] { return activeId > 0; })
        || !stillReserved || activeId != reserved.value.id) return 9;

    {
        ncs::StationDetailDialog other(detailResult.value, userB.value.id, facadeB);
        other.show();
        if (!waitUntil([&] {
                QPushButton *reservedButton = button(other, QStringLiteral("已预约"));
                return hasLabel(other, QStringLiteral("已被预约"))
                    && reservedButton && !reservedButton->isEnabled();
            })) return 10;
        other.close();
    }

    bool started = false;
    QObject::connect(&facade, &ncs::UserClientFacade::chargeStarted,
                     [&](const ncs::ChargingRecord &record) {
                         started = record.id == reserved.value.id
                             && record.status == ncs::ChargingOrderStatus::Charging;
                     });
    facade.startCharge(userA.value.id, 1, reserved.value.id);
    if (!waitUntil([&] { return started; })) return 11;

    auto chargingDetail = stationService.detail(1);
    if (!chargingDetail.success) return 12;
    {
        ncs::StationDetailDialog ownCharging(chargingDetail.value, userA.value.id, facade);
        ownCharging.show();
        if (!waitUntil([&] {
                return hasLabel(ownCharging, QStringLiteral("正在充电"))
                    && button(ownCharging, QStringLiteral("查看充电"));
            })) return 13;
        ownCharging.close();
    }

    facade.disconnectFromServer();
    facadeB.disconnectFromServer();
    server.shutdown();
    return 0;
}
