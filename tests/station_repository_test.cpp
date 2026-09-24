#include "database/database_manager.h"
#include "repository/station_repository.h"

#include <QCoreApplication>
#include <QTemporaryDir>

#include <cmath>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::DatabaseManager database(directory.filePath(QStringLiteral("station.db")));
    if (!database.initialize()) return 1;

    ncs::StationRepository repository(database);
    QVector<ncs::Station> stations;
    QString error;
    if (!repository.list(&stations, &error) || stations.isEmpty()) return 2;

    const ncs::Station expected = stations.first();
    ncs::Station actual;
    bool found = false;
    if (!repository.findById(expected.id, &actual, &found, &error) || !found) return 3;
    if (actual.id != expected.id || actual.name != expected.name
        || actual.address != expected.address
        || actual.totalSlots != expected.totalSlots
        || actual.idleSlots != expected.idleSlots
        || actual.chargerCount != expected.chargerCount
        || actual.reviewCount != expected.reviewCount
        || std::abs(actual.longitude - expected.longitude) > 1e-9
        || std::abs(actual.latitude - expected.latitude) > 1e-9
        || std::abs(actual.price - expected.price) > 1e-9) return 4;

    if (!repository.findById(999999, &actual, &found, &error) || found) return 5;
    return 0;
}
