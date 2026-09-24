#include "database/database_manager.h"
#include "model/station.h"
#include "repository/station_repository.h"
#include "service/station_service.h"

#include <QCoreApplication>
#include <QSqlQuery>
#include <QTemporaryDir>

namespace {

bool exec(QSqlQuery &query, const QString &sql)
{
    return query.exec(sql);
}

bool containsReason(const ncs::StationRecommendation &item, const QString &needle)
{
    for (const QString &reason : item.reasons) {
        if (reason.contains(needle)) return true;
    }
    return false;
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::DatabaseManager database(directory.filePath(QStringLiteral("recommend.db")));
    if (!database.initialize()) return 1;

    QSqlQuery query(database.connection());
    if (!exec(query, QStringLiteral("DELETE FROM load_prediction"))
        || !exec(query, QStringLiteral("DELETE FROM charging_order"))
        || !exec(query, QStringLiteral("DELETE FROM recharge_log"))
        || !exec(query, QStringLiteral("DELETE FROM charger"))
        || !exec(query, QStringLiteral("DELETE FROM station"))
        || !exec(query, QStringLiteral("DELETE FROM user"))
        || !exec(query, QStringLiteral(
            "INSERT INTO user(id,phone,nickname,balance,status,created_at,username) "
            "VALUES(1,'13800139001','推荐用户',80,1,'2026-09-08 09:00:00.000','rec_user')"))
        || !exec(query, QStringLiteral(
            "INSERT INTO station(id,name,address,longitude,latitude,price,total_slots,created_at) VALUES"
            "(1,'近距快充站','近地址',116.31,39.90,1.80,2,'2026-09-08 09:00:00.000'),"
            "(2,'常用平价站','常用地址',116.32,39.90,1.00,2,'2026-09-08 09:00:00.000'),"
            "(3,'远距备用站','远地址',116.55,39.90,1.20,1,'2026-09-08 09:00:00.000')"))
        || !exec(query, QStringLiteral(
            "INSERT INTO charger(id,station_id,code,type,power_kw,status,total_count,total_minutes,created_at) VALUES"
            "(1,1,'N-1',1,120,0,0,0,'2026-09-08 09:00:00.000'),"
            "(2,1,'N-2',1,120,1,0,0,'2026-09-08 09:00:00.000'),"
            "(3,2,'P-1',1,90,0,0,0,'2026-09-08 09:00:00.000'),"
            "(4,2,'P-2',0,7,0,0,0,'2026-09-08 09:00:00.000'),"
            "(5,3,'F-1',1,120,0,0,0,'2026-09-08 09:00:00.000')"))
        || !exec(query, QStringLiteral(
            "INSERT INTO charging_order(order_no,user_id,charger_id,station_id,status,"
            "reserved_at,expire_at,start_time,end_time,energy,amount,price_per_kwh,"
            "power_kw,created_at,updated_at) VALUES"
            "('R-1',1,3,2,2,'','',"
            "'2026-09-01 10:00:00.000','2026-09-01 11:00:00.000',20,20,1,90,"
            "'2026-09-01 10:00:00.000','2026-09-01 11:00:00.000'),"
            "('R-2',1,4,2,2,'','',"
            "'2026-09-02 10:00:00.000','2026-09-02 11:00:00.000',18,18,1,7,"
            "'2026-09-02 10:00:00.000','2026-09-02 11:00:00.000')"))
        || !exec(query, QStringLiteral(
            "INSERT INTO load_prediction(station_id,generated_at,target_time,horizon_hours,"
            "predicted_energy,predicted_free_chargers,is_peak) VALUES"
            "(1,'2026-09-08 09:00:00.000','2026-09-08 10:00:00',24,90,0,1),"
            "(2,'2026-09-08 09:00:00.000','2026-09-08 10:00:00',24,10,2,0)"))) {
        return 2;
    }

    ncs::StationRepository repository(database);
    ncs::StationService service(repository);
    const auto result = service.recommend(1, 116.30, 39.90, true, 75.0);
    if (!result.success || result.value.size() != 3) return 3;
    if (result.value.at(0).station.id != 2) return 4;
    if (!containsReason(result.value.at(0), QStringLiteral("历史"))
        || !containsReason(result.value.at(0), QStringLiteral("预测"))
        || result.value.at(0).idleRate < 0.99) return 5;
    if (result.value.at(0).score < result.value.at(1).score
        || result.value.at(1).score < result.value.at(2).score) return 6;
    for (const auto &item : result.value) {
        if (item.station.id == 3 && containsReason(item, QStringLiteral("预测"))) return 7;
    }

    const auto invalid = service.recommend(1, 116.30, 39.90, true, 120.0);
    if (invalid.success) return 8;
    return 0;
}
