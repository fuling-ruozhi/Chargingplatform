#include "database/database_manager.h"
#include "repository/station_repository.h"
#include "service/station_service.h"

#include <QCoreApplication>
#include <QSqlQuery>
#include <QTemporaryDir>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::DatabaseManager database(directory.filePath(QStringLiteral("phase32.db")));
    if (!database.initialize()) return 1;
    ncs::StationRepository repository(database);
    ncs::StationService service(repository);
    const auto result = service.list();
    if (!result.success || result.value.isEmpty()) return 2;
    const ncs::Station &station = result.value.first();
    if (station.name != QStringLiteral("BIT充电站") || station.price != 1.2
        || station.totalSlots != 8 || station.idleSlots != 7) return 3;
    QSqlQuery query(database.connection());
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM station")) || !query.next()
        || query.value(0).toInt() != result.value.size()) return 4;
    return 0;
}
