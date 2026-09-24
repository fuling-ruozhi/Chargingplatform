#include "database/database_manager.h"
#include "repository/charge_repository.h"
#include "repository/station_repository.h"
#include "repository/user_repository.h"
#include "service/charge_service.h"
#include "service/station_service.h"
#include "service/user_service.h"

#include <QCoreApplication>
#include <QSqlQuery>
#include <QTemporaryDir>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::DatabaseManager database(directory.filePath(QStringLiteral("phase4-flow.db")));
    if (!database.initialize()) return 1;

    ncs::UserRepository userRepository(database);
    ncs::UserService userService(database, userRepository);
    if (!userService.registerUser(QStringLiteral("phase4_user"),
                                  QStringLiteral("secret1")).success) return 2;
    const auto login = userService.login(QStringLiteral("phase4_user"),
                                         QStringLiteral("secret1"));
    if (!login.success) return 3;
    if (!userService.recharge(login.value.id, 10.0).success) return 12;

    ncs::StationRepository stationRepository(database);
    ncs::StationService stationService(stationRepository);
    const auto stations = stationService.list();
    if (!stations.success || stations.value.isEmpty()) return 4;
    auto detail = stationService.detail(stations.value.first().id);
    if (!detail.success || detail.value.chargers.size() < 2) return 5;
    const qint64 charger1 = detail.value.chargers.at(0).id;
    const qint64 charger2 = detail.value.chargers.at(1).id;

    QSqlQuery setup(database.connection());
    setup.prepare(QStringLiteral("UPDATE charger SET status=0 WHERE id IN (:first,:second)"));
    setup.bindValue(QStringLiteral(":first"), charger1);
    setup.bindValue(QStringLiteral(":second"), charger2);
    if (!setup.exec()) return 6;

    ncs::ChargeRepository chargeRepository(database);
    ncs::ChargeService chargeService(database, chargeRepository);
    const auto started = chargeService.start(login.value.id, charger1);
    if (!started.success || started.value.id <= 0) return 7;

    detail = stationService.detail(stations.value.first().id);
    if (!detail.success) return 8;
    ncs::ChargerStatus firstStatus = ncs::ChargerStatus::Fault;
    ncs::ChargerStatus secondStatus = ncs::ChargerStatus::Fault;
    for (const ncs::Charger &charger : detail.value.chargers) {
        if (charger.id == charger1) firstStatus = charger.status;
        if (charger.id == charger2) secondStatus = charger.status;
    }
    if (firstStatus != ncs::ChargerStatus::Using
        || secondStatus != ncs::ChargerStatus::Idle) return 9;

    const auto stopped = chargeService.stop(started.value.id);
    if (!stopped.success || stopped.value.endTime.isEmpty()
        || stopped.value.durationSeconds < 0 || stopped.value.energy < 0
        || stopped.value.cost < 0) return 10;

    QSqlQuery verify(database.connection());
    verify.prepare(QStringLiteral(
        "SELECT o.end_time,o.energy,o.amount,c1.status,c2.status "
        "FROM charging_order o "
        "JOIN charger c1 ON c1.id=o.charger_id "
        "JOIN charger c2 ON c2.id=:second WHERE o.id=:record"));
    verify.bindValue(QStringLiteral(":second"), charger2);
    verify.bindValue(QStringLiteral(":record"), started.value.id);
    if (!verify.exec() || !verify.next()
        || verify.value(0).toString().isEmpty()
        || verify.value(1).toDouble() < 0
        || verify.value(2).toDouble() < 0
        || verify.value(3).toInt() != static_cast<int>(ncs::ChargerStatus::Idle)
        || verify.value(4).toInt() != static_cast<int>(ncs::ChargerStatus::Idle)) return 11;
    return 0;
}
