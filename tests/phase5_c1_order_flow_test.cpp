#include "database/database_manager.h"
#include "model/business_error.h"
#include "repository/charge_repository.h"
#include "repository/user_repository.h"
#include "service/charge_service.h"
#include "service/user_service.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QSqlQuery>
#include <QTemporaryDir>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::DatabaseManager database(directory.filePath(QStringLiteral("c1-order-flow.db")));
    if (!database.initialize()) return 1;

    QSqlQuery setup(database.connection());
    if (!setup.exec(QStringLiteral("UPDATE charger SET status=0 WHERE id IN (1,2)"))
        || !setup.exec(QStringLiteral("UPDATE charger SET status=2 WHERE id=3"))) return 2;

    ncs::UserRepository userRepository(database);
    ncs::UserService userService(database, userRepository);
    const auto userA = userService.registerUser(QStringLiteral("order_user_a"),
                                                QStringLiteral("secret1"));
    const auto userB = userService.registerUser(QStringLiteral("order_user_b"),
                                                QStringLiteral("secret1"));
    if (!userA.success || !userB.success) return 3;
    if (!userService.recharge(userA.value.id, 10.0).success
        || !userService.recharge(userB.value.id, 10.0).success) return 29;

    ncs::ChargeRepository repository(database);
    ncs::ChargeService service(database, repository);

    const auto reserved = service.reserve(userA.value.id, 1);
    if (!reserved.success || reserved.value.id <= 0
        || reserved.value.status != ncs::ChargingOrderStatus::Reserved
        || !reserved.value.startTime.isEmpty() || reserved.value.expireAt.isEmpty()) return 4;

    QSqlQuery verify(database.connection());
    if (!verify.prepare(QStringLiteral(
            "SELECT o.status,c.status FROM charging_order o "
            "JOIN charger c ON c.id=o.charger_id WHERE o.id=:id"))) return 5;
    verify.bindValue(QStringLiteral(":id"), reserved.value.id);
    if (!verify.exec()
        || !verify.next() || verify.value(0).toInt() != 0 || verify.value(1).toInt() != 1) {
        return 5;
    }

    const auto sameUser = service.reserve(userA.value.id, 2);
    if (sameUser.success || sameUser.code != ncs::BusinessErrorCode::ActiveOrderExists) return 6;
    const auto sameCharger = service.reserve(userB.value.id, 1);
    if (sameCharger.success
        || sameCharger.code != ncs::BusinessErrorCode::ConcurrentReservationConflict) return 7;
    const auto fault = service.reserve(userB.value.id, 3);
    if (fault.success || fault.code != ncs::BusinessErrorCode::ChargerFault) return 8;
    const auto missingUser = service.reserve(999999, 2);
    if (missingUser.success
        || missingUser.code != ncs::BusinessErrorCode::UserNotFound) return 25;
    QSqlQuery activeCount(database.connection());
    if (!activeCount.exec(QStringLiteral(
            "SELECT COUNT(*) FROM charging_order WHERE status IN (0,1)"))
        || !activeCount.next() || activeCount.value(0).toInt() != 1) return 26;

    const auto activeReserved = service.active(userA.value.id);
    if (!activeReserved.success || activeReserved.value.id != reserved.value.id
        || activeReserved.value.status != ncs::ChargingOrderStatus::Reserved
        || activeReserved.value.stationName.isEmpty()
        || activeReserved.value.chargerCode.isEmpty()) return 9;
    const auto invalidCancelOwner = service.cancel(userB.value.id, reserved.value.id);
    if (invalidCancelOwner.success
        || invalidCancelOwner.code != ncs::BusinessErrorCode::InvalidOrderOwner) return 27;
    const auto invalidStartOwner = service.start(userB.value.id, 1, reserved.value.id);
    if (invalidStartOwner.success
        || invalidStartOwner.code != ncs::BusinessErrorCode::InvalidOrderOwner) return 28;

    const auto started = service.start(userA.value.id, 1, reserved.value.id);
    if (!started.success || started.value.id != reserved.value.id
        || started.value.status != ncs::ChargingOrderStatus::Charging
        || started.value.startTime.isEmpty()) return 10;
    const auto activeCharging = service.active(userA.value.id);
    if (!activeCharging.success
        || activeCharging.value.status != ncs::ChargingOrderStatus::Charging) return 11;

    const auto cancelCharging = service.cancel(userA.value.id, reserved.value.id);
    if (cancelCharging.success
        || cancelCharging.code != ncs::BusinessErrorCode::InvalidOrderState) return 12;

    const auto completed = service.stop(reserved.value.id);
    if (!completed.success
        || completed.value.status != ncs::ChargingOrderStatus::Completed
        || completed.value.endTime.isEmpty()) return 13;
    const auto restartCompleted = service.start(userA.value.id, 1, reserved.value.id);
    if (restartCompleted.success
        || restartCompleted.code != ncs::BusinessErrorCode::InvalidOrderState) return 14;

    const auto secondReservation = service.reserve(userA.value.id, 2);
    if (!secondReservation.success) return 15;
    const auto cancelled = service.cancel(userA.value.id, secondReservation.value.id);
    if (!cancelled.success
        || cancelled.value.status != ncs::ChargingOrderStatus::Cancelled) return 16;
    if (!verify.exec(QStringLiteral("SELECT status FROM charger WHERE id=2"))
        || !verify.next() || verify.value(0).toInt() != 0) return 17;
    const auto restartCancelled = service.start(userA.value.id, 2,
                                                secondReservation.value.id);
    if (restartCancelled.success
        || restartCancelled.code != ncs::BusinessErrorCode::InvalidOrderState) return 18;

    const auto expiring = service.reserve(userB.value.id, 2);
    if (!expiring.success) return 19;
    QSqlQuery expire(database.connection());
    expire.prepare(QStringLiteral(
        "UPDATE charging_order SET expire_at=:expired WHERE id=:id"));
    expire.bindValue(QStringLiteral(":expired"),
                     QDateTime::currentDateTimeUtc().addSecs(-1).toString(Qt::ISODateWithMs));
    expire.bindValue(QStringLiteral(":id"), expiring.value.id);
    if (!expire.exec()) return 20;

    const auto noActiveAfterExpiry = service.active(userB.value.id);
    if (!noActiveAfterExpiry.success || noActiveAfterExpiry.value.id != 0) return 21;
    QSqlQuery expiredState(database.connection());
    expiredState.prepare(QStringLiteral(
        "SELECT o.status,c.status FROM charging_order o "
        "JOIN charger c ON c.id=o.charger_id WHERE o.id=:id"));
    expiredState.bindValue(QStringLiteral(":id"), expiring.value.id);
    if (!expiredState.exec() || !expiredState.next()
        || expiredState.value(0).toInt() != 3 || expiredState.value(1).toInt() != 0) return 22;

    const auto noActive = service.active(userA.value.id);
    if (!noActive.success || noActive.value.id != 0) return 23;

    QSqlQuery counts(database.connection());
    if (!counts.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM charging_order "
            "WHERE user_id=:user_id AND status IN (0,1)"))) return 24;
    counts.bindValue(QStringLiteral(":user_id"), userA.value.id);
    if (!counts.exec() || !counts.next() || counts.value(0).toInt() != 0) return 24;
    return 0;
}
