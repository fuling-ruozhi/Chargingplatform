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
    ncs::DatabaseManager database(directory.filePath(QStringLiteral("order-source.db")));
    if (!database.initialize()) return 1;
    if (database.connection().tables().contains(QStringLiteral("charging_record"))) return 2;

    ncs::UserRepository users(database);
    ncs::UserService userService(database, users);
    const auto user = userService.registerUser(QStringLiteral("source_user"),
                                               QStringLiteral("secret1"));
    if (!user.success) return 3;
    if (!userService.recharge(user.value.id, 10.0).success) return 10;

    QSqlQuery charger(database.connection());
    if (!charger.exec(QStringLiteral(
            "SELECT id FROM charger WHERE status=0 ORDER BY id LIMIT 1"))
        || !charger.next()) return 4;
    const qint64 chargerId = charger.value(0).toLongLong();

    ncs::ChargeRepository repository(database);
    ncs::ChargeService service(database, repository);
    const auto reserved = service.reserve(user.value.id, chargerId);
    if (!reserved.success || reserved.value.status != ncs::ChargingOrderStatus::Reserved)
        return 5;
    const auto started = service.startReserved(user.value.id, reserved.value.id);
    if (!started.success || started.value.status != ncs::ChargingOrderStatus::Charging)
        return 6;
    const auto completed = service.stop(started.value.id, user.value.id);
    if (!completed.success || completed.value.status != ncs::ChargingOrderStatus::Completed)
        return 7;

    QSqlQuery order(database.connection());
    order.prepare(QStringLiteral(
        "SELECT status,end_time,energy,amount,station_name_snapshot,charger_code_snapshot "
        "FROM charging_order WHERE id=:id"));
    order.bindValue(QStringLiteral(":id"), completed.value.id);
    if (!order.exec() || !order.next() || order.value(0).toInt() != 2
        || order.value(1).toString().isEmpty() || order.value(2).toDouble() < 0
        || order.value(3).toDouble() < 0 || order.value(4).toString().isEmpty()
        || order.value(5).toString().isEmpty()) return 8;

    QSqlQuery indexes(database.connection());
    if (!indexes.exec(QStringLiteral(
            "SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND name IN "
            "('idx_order_one_active_user','idx_order_one_active_charger')"))
        || !indexes.next() || indexes.value(0).toInt() != 2) return 9;
    return 0;
}
