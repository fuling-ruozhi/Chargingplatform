#include "database/database_manager.h"
#include "repository/charge_repository.h"
#include "repository/user_repository.h"
#include "service/charge_service.h"
#include "service/user_service.h"

#include <QCoreApplication>
#include <QSqlQuery>
#include <QTemporaryDir>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::DatabaseManager database(directory.filePath(QStringLiteral("phase34.db")));
    if (!database.initialize()) return 1;
    ncs::UserRepository userRepository(database);
    ncs::UserService userService(database, userRepository);
    const auto user = userService.registerUser(QStringLiteral("charge_user"),
                                               QStringLiteral("secret1"));
    if (!user.success) return 2;
    if (!userService.recharge(user.value.id, 10.0).success) return 7;
    ncs::ChargeRepository chargeRepository(database);
    ncs::ChargeService chargeService(database, chargeRepository);
    const auto started = chargeService.start(user.value.id, 1);
    if (!started.success || started.value.id <= 0 || !started.value.endTime.isEmpty()) return 3;
    const auto duplicate = chargeService.start(user.value.id, 1);
    if (duplicate.success || duplicate.code != ncs::BusinessErrorCode::ChargerUnavailable) return 4;
    const auto stopped = chargeService.stop(started.value.id);
    if (!stopped.success || stopped.value.endTime.isEmpty()
        || stopped.value.energy < 0 || stopped.value.cost < 0) return 5;
    QSqlQuery query(database.connection());
    query.prepare(QStringLiteral(
        "SELECT o.end_time, o.energy, o.amount, c.status FROM charging_order o "
        "JOIN charger c ON c.id=o.charger_id WHERE o.id=:id"));
    query.bindValue(QStringLiteral(":id"), started.value.id);
    if (!query.exec() || !query.next() || query.value(0).toString().isEmpty()
        || query.value(1).toDouble() < 0
        || query.value(2).toDouble() < 0
        || query.value(3).toInt() != static_cast<int>(ncs::ChargerStatus::Idle)) return 6;
    return 0;
}
