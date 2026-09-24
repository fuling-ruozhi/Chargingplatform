#include "favorite_station_repository.h"

#include "database/database_manager.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

namespace ncs {

FavoriteStationRepository::FavoriteStationRepository(DatabaseManager &database)
    : database_(database) {}

bool FavoriteStationRepository::listByUser(qint64 userId, QVector<qint64> *stationIds,
                                           QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT station_id FROM user_favorite_station WHERE user_id=:user_id ORDER BY station_id"))) {
        *error = query.lastError().text(); return false;
    }
    query.bindValue(QStringLiteral(":user_id"), userId);
    if (!query.exec()) { *error = query.lastError().text(); return false; }
    stationIds->clear();
    while (query.next()) stationIds->append(query.value(0).toLongLong());
    return true;
}

bool FavoriteStationRepository::exists(qint64 userId, qint64 stationId, bool *result,
                                       QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT 1 FROM user_favorite_station WHERE user_id=:user_id AND station_id=:station_id"))) {
        *error = query.lastError().text(); return false;
    }
    query.bindValue(QStringLiteral(":user_id"), userId);
    query.bindValue(QStringLiteral(":station_id"), stationId);
    if (!query.exec()) { *error = query.lastError().text(); return false; }
    *result = query.next();
    return true;
}

bool FavoriteStationRepository::stationExists(qint64 stationId, bool *result,
                                              QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral("SELECT 1 FROM station WHERE id=:id"))) {
        *error = query.lastError().text(); return false;
    }
    query.bindValue(QStringLiteral(":id"), stationId);
    if (!query.exec()) { *error = query.lastError().text(); return false; }
    *result = query.next();
    return true;
}

bool FavoriteStationRepository::add(qint64 userId, qint64 stationId, bool *added,
                                    QString *error) const
{
    QSqlQuery query(database_.connection());
    // SQLite is the current database; this explicit ON CONFLICT form also maps cleanly to PostgreSQL.
    if (!query.prepare(QStringLiteral(
            "INSERT INTO user_favorite_station(user_id,station_id,created_at) "
            "VALUES(:user_id,:station_id,:created_at) "
            "ON CONFLICT(user_id,station_id) DO NOTHING"))) {
        *error = query.lastError().text(); return false;
    }
    query.bindValue(QStringLiteral(":user_id"), userId);
    query.bindValue(QStringLiteral(":station_id"), stationId);
    query.bindValue(QStringLiteral(":created_at"),
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (!query.exec()) { *error = query.lastError().text(); return false; }
    *added = query.numRowsAffected() == 1;
    return true;
}

bool FavoriteStationRepository::remove(qint64 userId, qint64 stationId, bool *removed,
                                       QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "DELETE FROM user_favorite_station WHERE user_id=:user_id AND station_id=:station_id"))) {
        *error = query.lastError().text(); return false;
    }
    query.bindValue(QStringLiteral(":user_id"), userId);
    query.bindValue(QStringLiteral(":station_id"), stationId);
    if (!query.exec()) { *error = query.lastError().text(); return false; }
    *removed = query.numRowsAffected() == 1;
    return true;
}

}
