#include "database/database_manager.h"
#include "repository/station_repository.h"
#include "service/station_service.h"
#include "util/geo_distance.h"
#include "util/navigation_url.h"

#include <QCoreApplication>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUrlQuery>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (!ncs::GeoDistance::validCoordinate(116.3, 39.9)
        || ncs::GeoDistance::validCoordinate(181.0, 39.9)) return 1;
    const double known = ncs::GeoDistance::haversineKm(116.3, 39.9, 116.4, 39.9);
    if (known < 8.4 || known > 8.7) return 2;
    const QUrl navigation = ncs::NavigationUrl::build(
        116.3, 39.9, 116.4, 39.95, QStringLiteral("目标站"));
    const QUrlQuery navigationQuery(navigation);
    if (navigation.scheme() != QStringLiteral("https")
        || navigation.host() != QStringLiteral("apis.map.qq.com")
        || navigation.path() != QStringLiteral("/uri/v1/routeplan")
        || navigationQuery.queryItemValue(QStringLiteral("fromcoord"))
            != QStringLiteral("39.900000,116.300000")   // 纬度在前（说明书 7.4 提醒）
        || navigationQuery.queryItemValue(QStringLiteral("tocoord"))
            != QStringLiteral("39.950000,116.400000")
        || !navigationQuery.hasQueryItem(QStringLiteral("from"))
        || !navigationQuery.hasQueryItem(QStringLiteral("to"))
        || !navigationQuery.hasQueryItem(QStringLiteral("type"))
        || !navigationQuery.hasQueryItem(QStringLiteral("referer"))) return 3;

    QTemporaryDir directory;
    ncs::DatabaseManager database(directory.filePath(QStringLiteral("station-geo.db")));
    if (!database.initialize()) return 4;
    QSqlQuery setup(database.connection());
    if (!setup.exec(QStringLiteral("DELETE FROM charging_order"))
        || !setup.exec(QStringLiteral("DELETE FROM charger"))
        || !setup.exec(QStringLiteral("DELETE FROM station"))
        || !setup.exec(QStringLiteral(
            "INSERT INTO station(id,name,address,longitude,latitude,price,total_slots) VALUES"
            "(1,'远站','远地址',116.50,39.90,1.5,1),"
            "(2,'近站','近地址',116.31,39.90,1.2,2),"
            "(3,'中站','中地址',116.40,39.90,1.3,1)"))
        || !setup.exec(QStringLiteral(
            "INSERT INTO charger(station_id,code,type,power_kw,status,total_count,total_minutes) "
            "VALUES(2,'FAST-01',1,60,0,12,360),(2,'SLOW-01',0,7,2,3,80),"
            "(1,'FAR-01',0,7,0,0,0),(3,'MID-01',1,120,1,8,900)"))) return 5;

    ncs::StationRepository repository(database);
    ncs::StationService service(repository);
    const auto stations = service.list(116.30, 39.90, true);
    if (!stations.success || stations.value.size() != 3
        || stations.value.at(0).name != QStringLiteral("近站")
        || stations.value.at(0).idleSlots != 1
        || stations.value.at(0).totalSlots != 2
        || stations.value.at(0).distanceKm >= stations.value.at(1).distanceKm)
        return 6;
    const auto detail = service.detail(2, 116.30, 39.90, true);
    if (!detail.success || detail.value.station.distanceKm <= 0.0
        || detail.value.chargers.size() != 2) return 7;
    const auto fast = detail.value.chargers.at(0);
    if (fast.code != QStringLiteral("FAST-01") || fast.type != 1
        || fast.powerKw != 60.0 || fast.totalCount != 12
        || fast.totalMinutes != 360) return 8;
    if (service.list(999.0, 39.9, true).success) return 9;
    return 0;
}
