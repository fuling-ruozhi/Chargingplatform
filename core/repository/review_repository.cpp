#include "review_repository.h"

#include "database/database_manager.h"

#include <QSqlError>
#include <QSqlQuery>

namespace ncs {
namespace {

void readReview(QSqlQuery &query, Review *review)
{
    review->id = query.value(0).toLongLong();
    review->orderId = query.value(1).toLongLong();
    review->userId = query.value(2).toLongLong();
    review->stationId = query.value(3).toLongLong();
    review->chargerId = query.value(4).toLongLong();
    review->environmentScore = query.value(5).toInt();
    review->queueScore = query.value(6).toInt();
    review->equipmentScore = query.value(7).toInt();
    review->parkingScore = query.value(8).toInt();
    review->overallScore = query.value(9).toDouble();
    review->createdAt = query.value(10).toString();
}

const QString reviewSelection = QStringLiteral(
    "SELECT id,order_id,user_id,station_id,charger_id,environment_score,queue_score,"
    "equipment_score,parking_score,overall_score,created_at FROM charging_review ");

}

ReviewRepository::ReviewRepository(DatabaseManager &database) : database_(database) {}

bool ReviewRepository::createReview(const Review &review, Review *created, QString *error,
                                    bool *duplicateOrder) const
{
    if (duplicateOrder) *duplicateOrder = false;
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "INSERT INTO charging_review(order_id,user_id,station_id,charger_id,"
            "environment_score,queue_score,equipment_score,parking_score,overall_score) "
            "VALUES(:order_id,:user_id,:station_id,:charger_id,:environment,:queue,"
            ":equipment,:parking,:overall)"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":order_id"), review.orderId);
    query.bindValue(QStringLiteral(":user_id"), review.userId);
    query.bindValue(QStringLiteral(":station_id"), review.stationId);
    query.bindValue(QStringLiteral(":charger_id"), review.chargerId);
    query.bindValue(QStringLiteral(":environment"), review.environmentScore);
    query.bindValue(QStringLiteral(":queue"), review.queueScore);
    query.bindValue(QStringLiteral(":equipment"), review.equipmentScore);
    query.bindValue(QStringLiteral(":parking"), review.parkingScore);
    query.bindValue(QStringLiteral(":overall"), review.overallScore);
    if (!query.exec()) {
        *error = query.lastError().text();
        const QSqlError sqlError = query.lastError();
        const QString details = sqlError.databaseText() + QLatin1Char(' ')
            + sqlError.driverText();
        if (details.contains(QStringLiteral("charging_review.order_id"), Qt::CaseInsensitive)
            && duplicateOrder) {
            *duplicateOrder = true;
        }
        return false;
    }
    bool found = false;
    return findByOrderId(review.orderId, created, &found, error) && found;
}

bool ReviewRepository::findByOrderId(qint64 orderId, Review *review, bool *found,
                                     QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(reviewSelection + QStringLiteral("WHERE order_id=:order_id"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":order_id"), orderId);
    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }
    *found = query.next();
    if (*found) readReview(query, review);
    return true;
}

bool ReviewRepository::getStationRatingSummary(qint64 stationId,
                                               StationRatingSummary *summary,
                                               QString *error) const
{
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT COUNT(*),COALESCE(AVG(overall_score),0),"
            "COALESCE(AVG(environment_score),0),COALESCE(AVG(queue_score),0),"
            "COALESCE(AVG(equipment_score),0),COALESCE(AVG(parking_score),0) "
            "FROM charging_review WHERE station_id=:station_id"))) {
        *error = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":station_id"), stationId);
    if (!query.exec() || !query.next()) {
        *error = query.lastError().text();
        return false;
    }
    summary->reviewCount = query.value(0).toLongLong();
    summary->averageScore = query.value(1).toDouble();
    summary->environmentAverage = query.value(2).toDouble();
    summary->queueAverage = query.value(3).toDouble();
    summary->equipmentAverage = query.value(4).toDouble();
    summary->parkingAverage = query.value(5).toDouble();
    return true;
}

}
