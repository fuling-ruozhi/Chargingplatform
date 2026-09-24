#include "prediction_repository.h"

#include "database/database_manager.h"
#include "util/date_time_storage.h"

#include <QSqlError>
#include <QSqlQuery>

namespace ncs {
namespace {

bool execute(QSqlQuery &query, QString *error)
{
    if (query.exec()) {
        return true;
    }
    *error = query.lastError().text();
    return false;
}

}

bool PredictionRepository::list(PredictionList *result, QString *error) const
{
    result->clear();
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT p.id, p.station_id, s.name, p.generated_at, p.target_time,"
            " p.horizon_hours, p.predicted_energy, p.predicted_free_chargers,"
            " p.is_peak"
            " FROM load_prediction p"
            " LEFT JOIN station s ON s.id = p.station_id"
            " ORDER BY p.station_id, p.horizon_hours, p.target_time, p.id"))) {
        *error = query.lastError().text();
        return false;
    }
    if (!execute(query, error)) {
        return false;
    }
    while (query.next()) {
        result->append({query.value(0).toLongLong(),
                        query.value(1).toLongLong(),
                        query.value(2).toString(),
                        query.value(3).toString(),
                        query.value(4).toString(),
                        query.value(5).toInt(),
                        query.value(6).toDouble(),
                        query.value(7).toInt(),
                        query.value(8).toInt() != 0});
    }
    return true;
}

bool PredictionRepository::latestGeneratedAt(QString *generatedAt,
                                             QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT MAX(generated_at) FROM load_prediction"))) {
        *error = query.lastError().text();
        return false;
    }
    if (!execute(query, error) || !query.next()) {
        if (error->isEmpty()) {
            *error = QStringLiteral("Missing prediction aggregate");
        }
        return false;
    }
    *generatedAt = query.value(0).toString();
    return true;
}

bool PredictionRepository::hourlyActual(int hours, QVector<HourlyLoad> *result,
                                        QString *error) const
{
    result->clear();
    // BR-09：end_time 为本地时间文本（DateTimeStorage 约定）。窗口起点在 C++ 端
    // 按同一口径预计算后直接对原始列做范围比较，避免逐行 datetime() 阻止索引。
    const QString windowStart = DateTimeStorage::toText(
        QDateTime::currentDateTime().addSecs(-hours * 3600));
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT o.station_id, s.name,"
            " strftime('%Y-%m-%d %H:00', o.end_time)"
            " AS bucket, COALESCE(SUM(o.energy),0)"
            " FROM charging_order o"
            " LEFT JOIN station s ON s.id = o.station_id"
            " WHERE o.status = :completed AND o.end_time IS NOT NULL"
            " AND o.end_time >= :window_start"
            " GROUP BY o.station_id, bucket ORDER BY o.station_id, bucket"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":completed"), 2);
    query.bindValue(QStringLiteral(":window_start"), windowStart);
    if (!execute(query, error)) {
        return false;
    }
    while (query.next()) {
        result->append({query.value(0).toLongLong(), query.value(1).toString(),
                        query.value(2).toString(), query.value(3).toDouble()});
    }
    return true;
}

}
