#include "seed_data.h"

#include "seed_data_history.h"
#include "util/password_hasher.h"

#include <QList>
#include <QSqlError>
#include <QSqlQuery>
#include <QtGlobal>
#include <QVariant>

namespace ncs {
namespace {

struct StationSeed
{
    const char *name;
    const char *address;
    double longitude;
    double latitude;
    double price;
};

bool queryError(const QSqlQuery &query, const QString &context, QString *error)
{
    *error = QStringLiteral("%1: %2").arg(context, query.lastError().text());
    return false;
}

bool seedAdmin(QSqlDatabase &database, QString *error)
{
    QSqlQuery query(database);
    if (!query.prepare(QStringLiteral(
            "SELECT id FROM admin WHERE username=:username"))) {
        return queryError(query, QStringLiteral("Cannot inspect admin seed"), error);
    }
    query.bindValue(QStringLiteral(":username"), QStringLiteral("admin"));
    if (!query.exec()) {
        return queryError(query, QStringLiteral("Cannot inspect admin seed"), error);
    }
    if (query.next()) return true;

    const QString salt = PasswordHasher::makeSalt();
    if (!query.prepare(QStringLiteral(
            "INSERT INTO admin(username,password_hash,salt,created_at) "
            "VALUES(:username,:password_hash,:salt,:created_at)"))) {
        return queryError(query, QStringLiteral("Cannot prepare admin seed"), error);
    }
    query.bindValue(QStringLiteral(":username"), QStringLiteral("admin"));
    const QByteArray configuredPassword = qgetenv("NCS_DEFAULT_ADMIN_PASSWORD");
    const QString defaultPassword = configuredPassword.isEmpty()
        ? QStringLiteral("123456")
        : QString::fromUtf8(configuredPassword);
    query.bindValue(QStringLiteral(":password_hash"),
                    PasswordHasher::hash(defaultPassword, salt));
    query.bindValue(QStringLiteral(":salt"), salt);
    query.bindValue(QStringLiteral(":created_at"),
                    QStringLiteral("2026-08-01 00:00:00.000"));
    if (!query.exec()) {
        return queryError(query, QStringLiteral("Cannot seed default admin"), error);
    }
    return true;
}

bool databaseHasCatalog(QSqlDatabase &database, bool *hasCatalog, QString *error)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM station")) || !query.next()) {
        return queryError(query, QStringLiteral("Cannot inspect station catalog"), error);
    }
    *hasCatalog = query.value(0).toLongLong() > 0;
    return true;
}

bool insertStation(QSqlDatabase &database, const StationSeed &seed,
                   qint64 *stationId, QString *error)
{
    QSqlQuery query(database);
    if (!query.prepare(QStringLiteral(
            "INSERT INTO station(name,address,longitude,latitude,price,total_slots,created_at) "
            "VALUES(:name,:address,:longitude,:latitude,:price,8,:created_at)"))) {
        return queryError(query, QStringLiteral("Cannot prepare station seed"), error);
    }
    query.bindValue(QStringLiteral(":name"), QString::fromUtf8(seed.name));
    query.bindValue(QStringLiteral(":address"), QString::fromUtf8(seed.address));
    query.bindValue(QStringLiteral(":longitude"), seed.longitude);
    query.bindValue(QStringLiteral(":latitude"), seed.latitude);
    query.bindValue(QStringLiteral(":price"), seed.price);
    query.bindValue(QStringLiteral(":created_at"),
                    QStringLiteral("2026-08-01 00:00:00.000"));
    if (!query.exec()) {
        return queryError(query, QStringLiteral("Cannot seed station"), error);
    }
    *stationId = query.lastInsertId().toLongLong();
    return true;
}

bool insertChargers(QSqlDatabase &database, int stationNumber, qint64 stationId,
                    const QString &stationName, double pricePerKwh,
                    QList<ChargerSeed> *chargers, QString *error)
{
    QSqlQuery query(database);
    if (!query.prepare(QStringLiteral(
            "INSERT INTO charger(station_id,code,type,power_kw,status,total_count,"
            "total_minutes,created_at) VALUES(:station_id,:code,:type,:power_kw,"
            ":status,0,0,:created_at)"))) {
        return queryError(query, QStringLiteral("Cannot prepare charger seed"), error);
    }
    for (int number = 1; number <= 8; ++number) {
        const bool fast = number % 2 == 0;
        const double power = fast ? (number % 4 == 0 ? 120.0 : 60.0) : 7.0;
        const int status = number == 3 ? 2 : 0;
        const QString code = QStringLiteral("NCS-%1-%2")
                                 .arg(stationNumber, 2, 10, QLatin1Char('0'))
                                 .arg(number, 2, 10, QLatin1Char('0'));
        query.bindValue(QStringLiteral(":station_id"), stationId);
        query.bindValue(QStringLiteral(":code"), code);
        query.bindValue(QStringLiteral(":type"), fast ? 1 : 0);
        query.bindValue(QStringLiteral(":power_kw"), power);
        query.bindValue(QStringLiteral(":status"), status);
        query.bindValue(QStringLiteral(":created_at"),
                        QStringLiteral("2026-08-01 00:00:00.000"));
        if (!query.exec()) {
            return queryError(query, QStringLiteral("Cannot seed charger"), error);
        }
        if (status == 0) {
            chargers->append({query.lastInsertId().toLongLong(), stationId,
                              stationName, code, power, pricePerKwh});
        }
    }
    return true;
}

bool seedCatalog(QSqlDatabase &database, QList<ChargerSeed> *chargers,
                 QString *error)
{
    const StationSeed stations[] = {
        {"BIT充电站", "北京理工大学中关村校区", 116.3220, 39.9623, 1.20},
        {"中关村软件园充电站", "海淀区东北旺西路8号", 116.2988, 40.0493, 1.35},
        {"国贸中心充电站", "朝阳区建国门外大街1号", 116.4598, 39.9097, 1.55},
        {"奥林匹克中心充电站", "朝阳区北辰东路15号", 116.3935, 39.9850, 1.40},
        {"北京南站充电站", "丰台区永外大街车站路12号", 116.3789, 39.8652, 1.30}
    };
    int stationNumber = 0;
    for (const StationSeed &seed : stations) {
        ++stationNumber;
        qint64 stationId = 0;
        if (!insertStation(database, seed, &stationId, error)
            || !insertChargers(database, stationNumber, stationId,
                               QString::fromUtf8(seed.name), seed.price,
                               chargers, error)) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool SeedData::populate(QSqlDatabase &database, QString *error)
{
    if (!seedAdmin(database, error)) return false;
    bool hasCatalog = false;
    if (!databaseHasCatalog(database, &hasCatalog, error) || hasCatalog) {
        return hasCatalog;
    }
    QList<ChargerSeed> chargers;
    return seedCatalog(database, &chargers, error)
        && seedDemoHistory(database, chargers, error);
}

}  // namespace ncs
