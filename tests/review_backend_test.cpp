#include "database/database_manager.h"
#include "model/business_error.h"
#include "repository/charge_repository.h"
#include "repository/review_repository.h"
#include "repository/user_repository.h"
#include "service/review_service.h"
#include "service/user_service.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVector>
#include <QUuid>

#include <cmath>

namespace {

bool closeTo(double actual, double expected)
{
    return std::abs(actual - expected) < 0.001;
}

bool prepareV6Database(const QString &path)
{
    ncs::DatabaseManager database(path);
    if (!database.initialize() || database.schemaVersion() != 10) return false;
    QSqlQuery query(database.connection());
    if (!query.exec(QStringLiteral("DROP TABLE charging_review"))
        || !query.exec(QStringLiteral("DELETE FROM schema_version"))
        || !query.exec(QStringLiteral(
            "INSERT INTO schema_version(version,applied_at) VALUES(6,'test-v6')"))) {
        return false;
    }
    query.clear();
    database.close();
    return true;
}

bool createOrder(QSqlDatabase database, qint64 userId, qint64 chargerId,
                 qint64 stationId, int status, qint64 *orderId)
{
    QSqlQuery query(database);
    if (!query.prepare(QStringLiteral(
            "INSERT INTO charging_order(order_no,user_id,charger_id,station_id,status,end_time) "
            "VALUES(:order_no,:user_id,:charger_id,:station_id,:status,:end_time)"))) {
        return false;
    }
    query.bindValue(QStringLiteral(":order_no"),
                    QStringLiteral("REVIEW-%1").arg(QUuid::createUuid().toString()));
    query.bindValue(QStringLiteral(":user_id"), userId);
    query.bindValue(QStringLiteral(":charger_id"), chargerId);
    query.bindValue(QStringLiteral(":station_id"), stationId);
    query.bindValue(QStringLiteral(":status"), status);
    query.bindValue(QStringLiteral(":end_time"),
                    status == 2 ? QStringLiteral("done") : QStringLiteral(""));
    if (!query.exec()) {
        qWarning() << "createOrder failed:" << query.lastError().text();
        return false;
    }
    *orderId = query.lastInsertId().toLongLong();
    return true;
}

bool hasReviewTable(QSqlDatabase database)
{
    return database.tables().contains(QStringLiteral("charging_review"));
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;

    const QString migrationPath = directory.filePath(QStringLiteral("migration.db"));
    if (!prepareV6Database(migrationPath)) return 2;
    ncs::DatabaseManager migrated(migrationPath);
    if (!migrated.initialize() || migrated.schemaVersion() != 10
        || !hasReviewTable(migrated.connection())) return 3;

    ncs::UserRepository users(migrated);
    ncs::UserService userService(migrated, users);
    const auto firstUser = userService.registerUser(QStringLiteral("review_user_1"),
                                                    QStringLiteral("secret1"));
    const auto secondUser = userService.registerUser(QStringLiteral("review_user_2"),
                                                     QStringLiteral("secret1"));
    if (!firstUser.success || !secondUser.success) return 4;

    QSqlQuery ids(migrated.connection());
    if (!ids.exec(QStringLiteral(
            "SELECT c.id,c.station_id FROM charger c JOIN station s ON s.id=c.station_id "
            "ORDER BY c.id LIMIT 2"))) return 5;
    QVector<QPair<qint64, qint64>> chargerStations;
    while (ids.next()) chargerStations.append({ids.value(0).toLongLong(), ids.value(1).toLongLong()});
    if (chargerStations.size() < 2) return 6;
    QSqlQuery emptyStationQuery(migrated.connection());
    emptyStationQuery.prepare(QStringLiteral(
        "SELECT id FROM station WHERE id<>:station_id ORDER BY id LIMIT 1"));
    emptyStationQuery.bindValue(QStringLiteral(":station_id"), chargerStations.at(0).second);
    if (!emptyStationQuery.exec() || !emptyStationQuery.next()) return 6;
    const qint64 emptyStationId = emptyStationQuery.value(0).toLongLong();

    qint64 completedOrder = 0;
    qint64 secondCompletedOrder = 0;
    qint64 pendingOrder = 0;
    if (!createOrder(migrated.connection(), firstUser.value.id, chargerStations.at(0).first,
                     chargerStations.at(0).second, 2, &completedOrder)
        || !createOrder(migrated.connection(), secondUser.value.id, chargerStations.at(0).first,
                        chargerStations.at(0).second, 2, &secondCompletedOrder)
        || !createOrder(migrated.connection(), firstUser.value.id, chargerStations.at(1).first,
                        chargerStations.at(1).second, 1, &pendingOrder)) return 7;

    ncs::ChargeRepository orders(migrated);
    ncs::ReviewRepository reviews(migrated);
    ncs::ReviewService service(migrated, orders, reviews);

    const auto missing = service.submitReview(firstUser.value.id, 999999, 5, 5, 5, 5);
    if (missing.success || missing.code != ncs::BusinessErrorCode::OrderNotFound) return 8;
    const auto otherOwner = service.submitReview(firstUser.value.id, secondCompletedOrder, 5, 5, 5, 5);
    if (otherOwner.success || otherOwner.code != ncs::BusinessErrorCode::InvalidOrderOwner) return 9;
    const auto unfinished = service.submitReview(firstUser.value.id, pendingOrder, 5, 5, 5, 5);
    if (unfinished.success || unfinished.code != ncs::BusinessErrorCode::InvalidOrderState) return 10;
    const auto zero = service.submitReview(firstUser.value.id, completedOrder, 0, 5, 5, 5);
    const auto six = service.submitReview(firstUser.value.id, completedOrder, 6, 5, 5, 5);
    if (zero.success || zero.code != ncs::BusinessErrorCode::InvalidReviewScore
        || six.success || six.code != ncs::BusinessErrorCode::InvalidReviewScore) return 11;

    const auto created = service.submitReview(firstUser.value.id, completedOrder, 5, 4, 3, 2);
    if (!created.success || !closeTo(created.value.overallScore, 3.5)) return 12;
    const auto duplicate = service.submitReview(firstUser.value.id, completedOrder, 5, 5, 5, 5);
    if (duplicate.success || duplicate.code != ncs::BusinessErrorCode::ReviewAlreadyExists) return 13;

    ncs::Review direct = created.value;
    ncs::Review ignored;
    QString error;
    if (reviews.createReview(direct, &ignored, &error)) return 14;

    const auto secondCreated = service.submitReview(secondUser.value.id, secondCompletedOrder,
                                                    4, 3, 5, 4);
    if (!secondCreated.success || !closeTo(secondCreated.value.overallScore, 4.0)) return 15;
    ncs::StationRatingSummary summary;
    if (!reviews.getStationRatingSummary(chargerStations.at(0).second, &summary, &error)
        || summary.reviewCount != 2
        || !closeTo(summary.averageScore, 3.75)
        || !closeTo(summary.environmentAverage, 4.5)
        || !closeTo(summary.queueAverage, 3.5)
        || !closeTo(summary.equipmentAverage, 4.0)
        || !closeTo(summary.parkingAverage, 3.0)) return 16;

    ncs::StationRatingSummary empty;
    if (!reviews.getStationRatingSummary(emptyStationId, &empty, &error)
        || empty.reviewCount != 0 || !closeTo(empty.averageScore, 0.0)
        || !closeTo(empty.environmentAverage, 0.0)
        || !closeTo(empty.queueAverage, 0.0)
        || !closeTo(empty.equipmentAverage, 0.0)
        || !closeTo(empty.parkingAverage, 0.0)) return 17;
    return 0;
}
