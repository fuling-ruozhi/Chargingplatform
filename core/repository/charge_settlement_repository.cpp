#include "charge_repository.h"

#include "database/database_manager.h"

#include <QSqlError>
#include <QSqlQuery>

namespace ncs {

bool ChargeRepository::finish(qint64 recordId, const QString &endTime, double energy,
                              double cost, double finalSoc, double debtAmount,
                              double balanceAfter, QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "UPDATE charging_order SET end_time=:end_time,energy=:energy,amount=:cost,"
            "final_soc=:final_soc,debt_amount=:debt,balance_after=:balance,"
            "status=2,updated_at=:end_time WHERE id=:id AND status=1 AND end_time=''"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":end_time"), endTime);
    query.bindValue(QStringLiteral(":energy"), energy);
    query.bindValue(QStringLiteral(":cost"), cost);
    query.bindValue(QStringLiteral(":final_soc"), finalSoc);
    query.bindValue(QStringLiteral(":debt"), debtAmount);
    query.bindValue(QStringLiteral(":balance"), balanceAfter);
    query.bindValue(QStringLiteral(":id"), recordId);
    if (!query.exec() || query.numRowsAffected() != 1) {
        *error = query.lastError().text().isEmpty()
            ? QStringLiteral("Active charging order changed") : query.lastError().text();
        return false;
    }
    return true;
}

bool ChargeRepository::settleCharger(qint64 chargerId, qint64 simulatedMinutes,
                                     QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "UPDATE charger SET status=0,total_count=total_count+1,"
            "total_minutes=total_minutes+:minutes WHERE id=:id AND status=1"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":minutes"), qMax<qint64>(0, simulatedMinutes));
    query.bindValue(QStringLiteral(":id"), chargerId);
    if (!query.exec() || query.numRowsAffected() != 1) {
        *error = query.lastError().text().isEmpty()
            ? QStringLiteral("Charging device state changed") : query.lastError().text();
        return false;
    }
    return true;
}

bool ChargeRepository::settleUserBalance(qint64 userId, double balanceAfter,
                                         QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "UPDATE user SET balance=:balance WHERE id=:id"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":balance"), balanceAfter);
    query.bindValue(QStringLiteral(":id"), userId);
    if (!query.exec() || query.numRowsAffected() != 1) {
        *error = query.lastError().text().isEmpty()
            ? QStringLiteral("Settlement user changed") : query.lastError().text();
        return false;
    }
    return true;
}

}
