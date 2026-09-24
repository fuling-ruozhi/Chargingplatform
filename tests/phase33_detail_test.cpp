#include "database/database_manager.h"
#include "repository/station_repository.h"
#include "service/station_service.h"

#include <QCoreApplication>
#include <QTemporaryDir>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::DatabaseManager database(directory.filePath(QStringLiteral("phase33.db")));
    if (!database.initialize()) return 1;
    ncs::StationRepository repository(database);
    ncs::StationService service(repository);
    const auto stations = service.list();
    if (!stations.success || stations.value.isEmpty()) return 2;
    const auto detail = service.detail(stations.value.first().id);
    if (!detail.success || detail.value.station.name != QStringLiteral("BIT充电站")) return 3;
    if (detail.value.chargers.size() != 8) return 4;
    if (detail.value.chargers.at(0).status != ncs::ChargerStatus::Idle
        || detail.value.chargers.at(1).status != ncs::ChargerStatus::Idle
        || detail.value.chargers.at(2).status != ncs::ChargerStatus::Fault
        || detail.value.chargers.at(0).code != QStringLiteral("NCS-01-01")) return 5;
    const auto missing = service.detail(999999);
    if (missing.success || missing.code != ncs::BusinessErrorCode::StationNotFound) return 6;
    return 0;
}
