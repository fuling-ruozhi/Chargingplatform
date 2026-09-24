#include "config/charge_config.h"
#include "database/database_manager.h"
#include "repository/charge_repository.h"
#include "repository/user_repository.h"
#include "service/charge_service.h"
#include "service/user_service.h"
#include "util/charge_calculator.h"
#include "util/date_time_storage.h"

#include <QCoreApplication>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimeZone>

#include <cmath>

namespace {

bool closeTo(double actual, double expected, double tolerance = 0.0001)
{
    return std::abs(actual - expected) <= tolerance;
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QDateTime start(QDate(2026, 9, 3), QTime(10, 0), QTimeZone::systemTimeZone());
    const auto metrics = ncs::ChargeCalculator::calculate(
        start, start.addSecs(30), 60.0, 1.2, 60, 20.0, 60.0);
    if (!closeTo(metrics.elapsedRealSeconds, 30.0)
        || metrics.elapsedSimSeconds != 1800
        || !closeTo(metrics.energyKwh, 30.0)
        || !closeTo(metrics.amount, 36.0)
        || !closeTo(metrics.soc, 70.0)) return 1;
    const auto later = ncs::ChargeCalculator::calculate(
        start, start.addSecs(60), 60.0, 1.2, 60, 20.0, 60.0);
    if (later.energyKwh <= metrics.energyKwh || later.amount <= metrics.amount
        || later.elapsedSimSeconds <= metrics.elapsedSimSeconds) return 2;

    QTemporaryDir directory;
    ncs::DatabaseManager database(directory.filePath(QStringLiteral("billing.db")));
    if (!database.initialize()) return 3;
    ncs::UserRepository users(database);
    ncs::UserService userService(database, users);
    const auto user = userService.registerUser(QStringLiteral("billing_user"),
                                               QStringLiteral("secret1"));
    if (!user.success || !userService.recharge(user.value.id, 100.0).success) return 4;
    QSqlQuery charger(database.connection());
    if (!charger.exec(QStringLiteral("SELECT id FROM charger WHERE status=0 LIMIT 1"))
        || !charger.next()) return 5;
    ncs::ChargeRepository orders(database);
    ncs::ChargeService service(database, orders);
    const auto reserved = service.reserve(user.value.id, charger.value(0).toLongLong());
    const auto charging = service.startReserved(user.value.id, reserved.value.id);
    if (!reserved.success || !charging.success) return 6;

    const QString backdated = ncs::DateTimeStorage::toText(
        ncs::DateTimeStorage::now().addSecs(-30));
    QSqlQuery update(database.connection());
    update.prepare(QStringLiteral(
        "UPDATE charging_order SET start_time=:start WHERE id=:id"));
    update.bindValue(QStringLiteral(":start"), backdated);
    update.bindValue(QStringLiteral(":id"), charging.value.id);
    if (!update.exec()) return 7;
    const auto settled = service.stop(charging.value.id, user.value.id);
    if (!settled.success || settled.value.durationSeconds < 1800
        || settled.value.energy < 3.49 || settled.value.energy > 3.52
        || settled.value.cost != 4.20 || settled.value.finalSoc <= 20.0) return 8;
    return 0;
}
