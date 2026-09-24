#include "database/database_manager.h"
#include "model/business_error.h"
#include "model/charger.h"
#include "repository/charger_repository.h"
#include "repository/station_repository.h"
#include "service/charger_service.h"

#include <QCoreApplication>
#include <QSqlQuery>
#include <QTemporaryDir>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::DatabaseManager database(directory.filePath(QStringLiteral("station.db")));
    if (!database.initialize()) return 1;
    ncs::StationRepository stations(database);
    QVector<ncs::Station> list;
    QString error;
    if (!stations.list(&list, &error) || list.size() != 5) return 2;
    QSqlQuery setFault(database.connection());
    if (!setFault.prepare(QStringLiteral(
            "UPDATE charger SET status=:status WHERE station_id=:station_id"))) return 11;
    setFault.bindValue(QStringLiteral(":status"),
                       static_cast<int>(ncs::ChargerStatus::Fault));
    setFault.bindValue(QStringLiteral(":station_id"), 1);
    if (!setFault.exec()) return 12;
    if (!stations.list(&list, &error) || list.isEmpty()
        || list.first().id != 1 || list.first().idleSlots != 0
        || list.first().chargerCount <= 0) return 13;
    ncs::Station created;
    ncs::Station input; input.name = QStringLiteral("A06站"); input.address = QStringLiteral("测试地址");
    input.longitude = 116.3; input.latitude = 39.9; input.price = 1.5; input.totalSlots = 3;
    if (!stations.insert(input, &created, &error)) return 3;
    input.id = created.id; input.name = QStringLiteral("A06新站"); ncs::Station updated; bool found = false;
    if (!stations.update(input, &updated, &found, &error) || !found || updated.name != input.name) return 4;
    ncs::ChargerRepository chargerRepository(database);
    ncs::ChargerService chargerService(chargerRepository);
    const auto batch = chargerService.batchCreate(created.id, QStringLiteral("A06"), 3, 0, 7.0);
    if (!batch.success || batch.value.size() != 3) return 5;
    int count = 0;
    if (!stations.chargerCount(created.id, &count, &error) || count != 3) return 6;
    found = false;
    if (stations.remove(created.id, &found, &error) || found) return 7;
    const auto rollback = chargerService.batchCreate(created.id, QStringLiteral("A06"), 2, 0, 7.0);
    if (rollback.success) return 8;
    if (!stations.chargerCount(created.id, &count, &error) || count != 3) return 9;
    QSqlQuery remove(database.connection());
    remove.prepare(QStringLiteral("DELETE FROM charger WHERE station_id=:id"));
    remove.bindValue(QStringLiteral(":id"), created.id);
    if (!remove.exec() || !stations.remove(created.id, &found, &error) || !found) return 10;
    return 0;
}
