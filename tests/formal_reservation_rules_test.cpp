#include "config/charge_config.h"
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
#include <QVector>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::DatabaseManager database(directory.filePath(QStringLiteral("reservation-rules.db")));
    if (!database.initialize() || ncs::ChargeConfig::minBalance() != 5.0
        || ncs::ChargeConfig::reservationMinutes() != 15) return 1;

    ncs::UserRepository users(database);
    ncs::UserService userService(database, users);
    const auto user = userService.registerUser(QStringLiteral("reservation_rules"),
                                               QStringLiteral("secret1"));
    if (!user.success) return 2;
    ncs::ChargeRepository orders(database);
    ncs::ChargeService service(database, orders);
    QSqlQuery state(database.connection());
    if (!state.exec(QStringLiteral("SELECT id FROM charger ORDER BY id LIMIT 2"))) return 17;
    QVector<qint64> chargerIds;
    while (state.next()) chargerIds.append(state.value(0).toLongLong());
    if (chargerIds.size() != 2) return 18;
    state.prepare(QStringLiteral("UPDATE charger SET status=0 WHERE id IN(:first,:second)"));
    state.bindValue(QStringLiteral(":first"), chargerIds.at(0));
    state.bindValue(QStringLiteral(":second"), chargerIds.at(1));
    if (!state.exec()) return 19;

    const auto noBalance = service.reserve(user.value.id, chargerIds.at(0));
    if (noBalance.success
        || noBalance.code != ncs::BusinessErrorCode::InsufficientBalance) return 3;
    if (!userService.recharge(user.value.id, 5.0).success) return 4;
    const auto reserved = service.reserve(user.value.id, chargerIds.at(0));
    if (!reserved.success || reserved.value.status != ncs::ChargingOrderStatus::Reserved)
        return 5;
    const QDateTime reservedAt = QDateTime::fromString(reserved.value.reservedAt,
                                                       Qt::ISODateWithMs);
    const QDateTime expireAt = QDateTime::fromString(reserved.value.expireAt,
                                                     Qt::ISODateWithMs);
    if (!reservedAt.isValid() || reservedAt.secsTo(expireAt) != 15 * 60) return 6;

    state.prepare(QStringLiteral("UPDATE user SET status=0 WHERE id=:id"));
    state.bindValue(QStringLiteral(":id"), user.value.id);
    if (!state.exec()) return 7;
    const auto frozenStart = service.startReserved(user.value.id, reserved.value.id);
    if (frozenStart.success || frozenStart.code != ncs::BusinessErrorCode::UserFrozen)
        return 8;
    if (!service.cancel(user.value.id, reserved.value.id).success) return 9;
    const auto frozenReserve = service.reserve(user.value.id, chargerIds.at(1));
    if (frozenReserve.success || frozenReserve.code != ncs::BusinessErrorCode::UserFrozen)
        return 10;

    state.prepare(QStringLiteral("UPDATE user SET status=1,balance=5 WHERE id=:id"));
    state.bindValue(QStringLiteral(":id"), user.value.id);
    if (!state.exec()) return 11;
    const auto second = service.reserve(user.value.id, chargerIds.at(1));
    if (!second.success) return 12;
    state.prepare(QStringLiteral("UPDATE user SET balance=4.99 WHERE id=:id"));
    state.bindValue(QStringLiteral(":id"), user.value.id);
    if (!state.exec()) return 13;
    const auto lowerBalanceStart = service.startReserved(user.value.id, second.value.id);
    if (lowerBalanceStart.success
        || lowerBalanceStart.code != ncs::BusinessErrorCode::InsufficientBalance) return 14;
    if (!service.cancel(user.value.id, second.value.id).success) return 15;

    state.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM charging_order WHERE user_id=:id AND status IN(0,1)"));
    state.bindValue(QStringLiteral(":id"), user.value.id);
    if (!state.exec()
        || !state.next() || state.value(0).toInt() != 0) return 16;
    return 0;
}
