#include "database/database_manager.h"
#include "model/charger.h"
#include "repository/charger_repository.h"
#include "service/charger_service.h"

#include <QCoreApplication>
#include <QSqlQuery>
#include <QTemporaryDir>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::DatabaseManager database(directory.filePath(QStringLiteral("charger.db")));
    if (!database.initialize()) return 1;

    ncs::ChargerRepository repository(database);
    ncs::ChargerService service(repository);
    const auto initial = service.list(QString(), -1);
    if (!initial.success || initial.value.size() != 40) return 2;
    ncs::Charger foundCharger;
    bool found = false;
    QString findError;
    if (!repository.findById(initial.value.first().id, &foundCharger, &found, &findError)
        || !found || foundCharger.id != initial.value.first().id
        || foundCharger.code != initial.value.first().code
        || foundCharger.stationName != initial.value.first().stationName
        || foundCharger.status != initial.value.first().status) return 16;
    if (!repository.findById(-1, &foundCharger, &found, &findError)
        || found) return 17;
    const auto stationFilter = service.list(QStringLiteral("BIT"), -1);
    if (!stationFilter.success || stationFilter.value.isEmpty()) return 3;
    const auto statusFilter = service.list(QString(), static_cast<int>(ncs::ChargerStatus::Idle));
    if (!statusFilter.success || statusFilter.value.isEmpty()) return 4;

    const auto created = service.create(1, QStringLiteral("A05-UNIQUE"), 0, 60.0);
    if (!created.success || created.value.code != QStringLiteral("A05-UNIQUE")
        || created.value.status != ncs::ChargerStatus::Idle) return 5;
    if (service.create(1, QStringLiteral("A05-UNIQUE"), 0, 60.0).success) return 6;
    const qint64 chargerId = created.value.id;

    const auto fault = service.markFault(chargerId);
    if (!fault.success || fault.value.status != ncs::ChargerStatus::Fault) return 7;
    if (service.markFault(chargerId).success) return 8;
    const auto restart = service.restart(chargerId);
    if (!restart.success) return 9;
    const auto recovered = service.recover(chargerId);
    if (!recovered.success || recovered.value.status != ncs::ChargerStatus::Idle) return 10;
    if (service.recover(chargerId).success) return 11;

    QSqlQuery setUsing(database.connection());
    setUsing.prepare(QStringLiteral("UPDATE charger SET status=1 WHERE id=:id"));
    setUsing.bindValue(QStringLiteral(":id"), chargerId);
    if (!setUsing.exec() || service.restart(chargerId).success
        || service.markFault(chargerId).success || service.remove(chargerId).success) return 12;

    QSqlQuery restore(database.connection());
    restore.prepare(QStringLiteral("UPDATE charger SET status=0 WHERE id=:id"));
    restore.bindValue(QStringLiteral(":id"), chargerId);
    if (!restore.exec()) return 13;
    const auto removed = service.remove(chargerId);
    if (!removed.success || service.list(QStringLiteral("A05-UNIQUE"), -1).value.size() != 0)
        return 14;
    if (service.create(9999, QStringLiteral("A05-NOSTATION"), 0, 7.0).code
        != ncs::BusinessErrorCode::StationNotFound) return 15;
    return 0;
}
