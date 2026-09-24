#include "database/database_manager.h"
#include "model/business_error.h"
#include "repository/charge_repository.h"
#include "repository/user_repository.h"
#include "service/charge_service.h"
#include "service/user_service.h"
#include "util/date_time_storage.h"
#include "util/money.h"

#include <QCoreApplication>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVector>

#include <cmath>

namespace {

bool closeTo(double actual, double expected)
{
    return std::abs(actual - expected) < 0.001;
}

bool orderState(QSqlDatabase db, qint64 orderId, int expectedStatus,
                const QString &expectedEnd)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT status,end_time FROM charging_order WHERE id=:id"));
    query.bindValue(QStringLiteral(":id"), orderId);
    return query.exec() && query.next()
        && query.value(0).toInt() == expectedStatus
        && query.value(1).toString() == expectedEnd;
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::DatabaseManager database(
        directory.filePath(QStringLiteral("formal-settlement.db")));
    if (!database.initialize()) return 1;

    QSqlQuery ids(database.connection());
    if (!ids.exec(QStringLiteral(
            "SELECT id FROM charger ORDER BY id LIMIT 2"))) return 2;
    QVector<qint64> chargerIds;
    while (ids.next()) chargerIds.append(ids.value(0).toLongLong());
    if (chargerIds.size() != 2) return 3;
    ids.prepare(QStringLiteral(
        "UPDATE charger SET status=0 WHERE id IN(:first,:second)"));
    ids.bindValue(QStringLiteral(":first"), chargerIds.at(0));
    ids.bindValue(QStringLiteral(":second"), chargerIds.at(1));
    if (!ids.exec()) return 20;

    ncs::UserRepository users(database);
    ncs::UserService userService(database, users);
    const auto lowUser = userService.registerUser(QStringLiteral("settle_low"),
                                                  QStringLiteral("secret1"));
    const auto fundedUser = userService.registerUser(QStringLiteral("settle_funded"),
                                                     QStringLiteral("secret1"));
    if (!lowUser.success || !fundedUser.success
        || !userService.recharge(lowUser.value.id, 5.0).success
        || !userService.recharge(fundedUser.value.id, 100.0).success) return 4;

    ncs::ChargeRepository orders(database);
    ncs::ChargeService service(database, orders);
    const auto lowReservation = service.reserve(lowUser.value.id, chargerIds.at(0));
    const auto lowCharging = service.startReserved(lowUser.value.id,
                                                   lowReservation.value.id);
    if (!lowReservation.success || !lowCharging.success) return 5;

    const QString longAgo = ncs::DateTimeStorage::toText(
        ncs::DateTimeStorage::now().addSecs(-600));
    QSqlQuery update(database.connection());
    update.prepare(QStringLiteral(
        "UPDATE charging_order SET start_time=:start WHERE id=:id"));
    update.bindValue(QStringLiteral(":start"), longAgo);
    update.bindValue(QStringLiteral(":id"), lowCharging.value.id);
    if (!update.exec()) return 6;

    QSqlQuery before(database.connection());
    before.prepare(QStringLiteral(
        "SELECT total_count,total_minutes FROM charger WHERE id=:id"));
    before.bindValue(QStringLiteral(":id"), chargerIds.at(0));
    if (!before.exec() || !before.next()) return 7;
    const int countBefore = before.value(0).toInt();
    const qint64 minutesBefore = before.value(1).toLongLong();

    QSqlQuery trigger(database.connection());
    if (!trigger.exec(QStringLiteral(
            "CREATE TRIGGER reject_settlement_balance BEFORE UPDATE OF balance ON user "
            "WHEN NEW.balance < OLD.balance BEGIN SELECT RAISE(ABORT,'injected'); END")))
        return 8;
    const auto failed = service.stop(lowCharging.value.id, lowUser.value.id);
    if (failed.success || failed.code != ncs::BusinessErrorCode::DatabaseError) return 9;
    QSqlQuery rolledBack(database.connection());
    rolledBack.prepare(QStringLiteral(
        "SELECT c.status,c.total_count,c.total_minutes,u.balance "
        "FROM charger c,user u WHERE c.id=:charger AND u.id=:user"));
    rolledBack.bindValue(QStringLiteral(":charger"), chargerIds.at(0));
    rolledBack.bindValue(QStringLiteral(":user"), lowUser.value.id);
    if (!rolledBack.exec() || !rolledBack.next()
        || !orderState(database.connection(), lowCharging.value.id, 1, QString())
        || rolledBack.value(0).toInt() != 1
        || rolledBack.value(1).toInt() != countBefore
        || rolledBack.value(2).toLongLong() != minutesBefore
        || !closeTo(rolledBack.value(3).toDouble(), 5.0)) return 10;
    if (!trigger.exec(QStringLiteral("DROP TRIGGER reject_settlement_balance"))) return 11;

    const auto settled = service.stop(lowCharging.value.id, lowUser.value.id);
    if (!settled.success || settled.value.status != ncs::ChargingOrderStatus::Completed
        || settled.value.cost <= 5.0 || settled.value.balanceAfter != 0.0
        || !closeTo(settled.value.debtAmount,
                    ncs::Money::round(settled.value.cost - 5.0))) return 12;
    QSqlQuery finalLow(database.connection());
    finalLow.prepare(QStringLiteral(
        "SELECT c.status,c.total_count,c.total_minutes,u.balance "
        "FROM charger c,user u WHERE c.id=:charger AND u.id=:user"));
    finalLow.bindValue(QStringLiteral(":charger"), chargerIds.at(0));
    finalLow.bindValue(QStringLiteral(":user"), lowUser.value.id);
    if (!finalLow.exec() || !finalLow.next()
        || finalLow.value(0).toInt() != 0
        || finalLow.value(1).toInt() != countBefore + 1
        || finalLow.value(2).toLongLong()
               != minutesBefore + settled.value.durationSeconds / 60
        || finalLow.value(3).toDouble() != 0.0) return 13;

    const auto retry = service.stop(lowCharging.value.id, lowUser.value.id);
    if (!retry.success || retry.value.endTime != settled.value.endTime
        || retry.value.durationSeconds != settled.value.durationSeconds
        || !closeTo(retry.value.cost, settled.value.cost)
        || !closeTo(retry.value.debtAmount, settled.value.debtAmount)) return 14;
    QSqlQuery noDuplicate(database.connection());
    noDuplicate.prepare(QStringLiteral(
        "SELECT total_count,total_minutes FROM charger WHERE id=:id"));
    noDuplicate.bindValue(QStringLiteral(":id"), chargerIds.at(0));
    if (!noDuplicate.exec() || !noDuplicate.next()
        || noDuplicate.value(0).toInt() != countBefore + 1
        || noDuplicate.value(1).toLongLong()
               != minutesBefore + settled.value.durationSeconds / 60) return 15;

    const auto forbidden = service.stop(lowCharging.value.id, fundedUser.value.id);
    if (forbidden.success
        || forbidden.code != ncs::BusinessErrorCode::InvalidOrderOwner) return 16;

    const auto fundedReservation = service.reserve(fundedUser.value.id, chargerIds.at(1));
    const auto fundedCharging = service.startReserved(
        fundedUser.value.id, fundedReservation.value.id);
    if (!fundedReservation.success || !fundedCharging.success) return 17;
    update.bindValue(QStringLiteral(":start"), ncs::DateTimeStorage::toText(
        ncs::DateTimeStorage::now().addSecs(-30)));
    update.bindValue(QStringLiteral(":id"), fundedCharging.value.id);
    if (!update.exec()) return 18;
    const auto fundedSettlement = service.stop(fundedCharging.value.id,
                                                fundedUser.value.id);
    if (!fundedSettlement.success || fundedSettlement.value.debtAmount != 0.0
        || !closeTo(fundedSettlement.value.balanceAfter,
                    ncs::Money::round(100.0 - fundedSettlement.value.cost))) return 19;

    return 0;
}
