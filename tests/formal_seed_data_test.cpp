#include "database/database_manager.h"
#include "util/charge_calculator.h"
#include "util/date_time_storage.h"
#include "util/money.h"
#include "util/password_hasher.h"

#include <QCoreApplication>
#include <QHash>
#include <QSet>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtMath>

#include <climits>

namespace {

constexpr int ExpectedUsers = 40;
constexpr int ExpectedStations = 5;
constexpr int ExpectedChargers = 40;
constexpr int ExpectedOrders = 1200;

bool countEquals(QSqlDatabase database, const QString &table, int expected)
{
    QSqlQuery query(database);
    return query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table))
        && query.next() && query.value(0).toInt() == expected;
}

QString snapshot(QSqlDatabase database)
{
    const QStringList statements{
        "SELECT name,address,longitude,latitude,price,total_slots FROM station ORDER BY id",
        "SELECT station_id,code,type,power_kw,status FROM charger ORDER BY id",
        "SELECT order_no,user_id,charger_id,station_id,status,reserved_at,expire_at,start_time,"
        "end_time,energy,amount,balance_after,price_per_kwh,power_kw,time_scale,"
        "initial_soc,final_soc,station_name_snapshot,charger_code_snapshot,legacy_source,"
        "legacy_id FROM charging_order ORDER BY id",
        "SELECT phone,nickname,balance,status,username FROM user ORDER BY id"};
    QStringList rows;
    for (const QString &statement : statements) {
        QSqlQuery query(database);
        if (!query.exec(statement)) return {};
        while (query.next()) {
            QStringList values;
            for (int column = 0; column < query.record().count(); ++column) {
                values.append(query.value(column).toString());
            }
            rows.append(values.join(QLatin1Char('|')));
        }
        rows.append(QStringLiteral("--"));
    }
    return rows.join(QLatin1Char('\n'));
}

bool verifyUsers(QSqlDatabase database)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(
            "SELECT COUNT(*),COUNT(DISTINCT phone),COUNT(DISTINCT username),"
            "SUM(CASE WHEN length(phone)=11 AND substr(phone,1,1)='1' "
            "AND phone NOT GLOB '*[^0-9]*' THEN 1 ELSE 0 END) FROM user"))
        || !query.next()) return false;
    if (query.value(0).toInt() != ExpectedUsers
        || query.value(1).toInt() != ExpectedUsers
        || query.value(2).toInt() != ExpectedUsers
        || query.value(3).toInt() != ExpectedUsers) return false;

    QSqlQuery fixed(database);
    return fixed.exec(QStringLiteral(
               "SELECT phone,username,status,balance FROM user "
               "WHERE username='formal-seed-user'"))
        && fixed.next()
        && fixed.value(0).toString() == QStringLiteral("13900000000")
        && fixed.value(1).toString() == QStringLiteral("formal-seed-user")
        && fixed.value(2).toInt() == 1
        && fixed.value(3).toDouble() >= 0.0;
}

bool verifyCatalog(QSqlDatabase database)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(
            "SELECT s.total_slots,COUNT(c.id),"
            "SUM(CASE WHEN c.type=0 THEN 1 ELSE 0 END),"
            "SUM(CASE WHEN c.type=1 THEN 1 ELSE 0 END),"
            "SUM(CASE WHEN c.status=2 THEN 1 ELSE 0 END) "
            "FROM station s JOIN charger c ON c.station_id=s.id "
            "GROUP BY s.id ORDER BY s.id"))) return false;
    int stations = 0;
    while (query.next()) {
        ++stations;
        if (query.value(0).toInt() != 8 || query.value(1).toInt() != 8
            || query.value(2).toInt() != 4 || query.value(3).toInt() != 4
            || query.value(4).toInt() != 1) return false;
    }
    return stations == ExpectedStations;
}

bool verifyOrders(QSqlDatabase database)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(
            "SELECT o.status,o.user_id,o.charger_id,o.start_time,o.end_time,o.energy,o.amount,"
            "o.price_per_kwh,o.power_kw,o.time_scale,o.station_id,c.station_id,"
            "o.station_name_snapshot,s.name,o.charger_code_snapshot,c.code,o.legacy_source "
            "FROM charging_order o JOIN charger c ON c.id=o.charger_id "
            "JOIN station s ON s.id=o.station_id ORDER BY o.charger_id,o.start_time,o.id"))) {
        return false;
    }
    int orders = 0;
    QDate first;
    QDate last;
    QSet<qint64> users;
    QSet<qint64> stations;
    QSet<qint64> chargers;
    QHash<qint64, QDateTime> previousEnd;
    while (query.next()) {
        ++orders;
        const qint64 userId = query.value(1).toLongLong();
        const qint64 chargerId = query.value(2).toLongLong();
        const QDateTime start = ncs::DateTimeStorage::fromText(query.value(3).toString());
        const QDateTime end = ncs::DateTimeStorage::fromText(query.value(4).toString());
        const double energy = query.value(5).toDouble();
        const double amount = query.value(6).toDouble();
        const double price = query.value(7).toDouble();
        const double power = query.value(8).toDouble();
        const int scale = query.value(9).toInt();
        const qint64 stationId = query.value(10).toLongLong();
        const qint64 chargerStationId = query.value(11).toLongLong();
        if (query.value(0).toInt() != 2 || !start.isValid() || end <= start
            || energy <= 0.0 || amount < 0.0
            || qAbs(energy - power * start.secsTo(end) * scale / 3600.0) > 0.000001
            || amount != ncs::Money::round(energy * price)
            || stationId != chargerStationId
            || query.value(13).toString() != query.value(12).toString()
            || query.value(15).toString() != query.value(14).toString()
            || query.value(16).toString() != QStringLiteral("formal_seed_v1")) return false;
        if (previousEnd.contains(chargerId) && previousEnd.value(chargerId) > start) return false;
        previousEnd.insert(chargerId, end);
        users.insert(userId);
        stations.insert(stationId);
        chargers.insert(chargerId);
        if (!first.isValid()) first = start.date();
        last = qMax(last, start.date());
    }
    if (orders != ExpectedOrders || !first.isValid() || first.daysTo(last) < 29
        || users.size() != ExpectedUsers || stations.size() != ExpectedStations
        || chargers.size() < 32) return false;

    QSqlQuery balance(database);
    if (!balance.exec(QStringLiteral(
            "SELECT u.id,u.balance,o.balance_after FROM user u "
            "JOIN charging_order o ON o.user_id=u.id "
            "WHERE o.legacy_source='formal_seed_v1' AND o.id=("
            "SELECT MAX(latest.id) FROM charging_order latest "
            "WHERE latest.user_id=u.id AND latest.legacy_source='formal_seed_v1')"))) {
        return false;
    }
    int usersWithLatestBalance = 0;
    while (balance.next()) {
        ++usersWithLatestBalance;
        if (qAbs(balance.value(1).toDouble() - balance.value(2).toDouble()) > 0.000001
            || balance.value(1).toDouble() < 0.0) return false;
    }
    return usersWithLatestBalance == ExpectedUsers;
}

bool verifyDistribution(QSqlDatabase database)
{
    QSqlQuery daily(database);
    if (!daily.exec(QStringLiteral(
            "SELECT date(start_time),COUNT(*) FROM charging_order "
            "WHERE legacy_source='formal_seed_v1' GROUP BY date(start_time) "
            "ORDER BY date(start_time)"))) return false;
    int days = 0;
    int minimum = INT_MAX;
    int maximum = 0;
    int total = 0;
    while (daily.next()) {
        ++days;
        const int count = daily.value(1).toInt();
        minimum = qMin(minimum, count);
        maximum = qMax(maximum, count);
        total += count;
    }
    if (days != 30 || total != ExpectedOrders || minimum < 30 || maximum > 60
        || minimum == maximum) return false;

    QSqlQuery peaks(database);
    if (!peaks.exec(QStringLiteral(
            "SELECT SUM(CASE WHEN CAST(strftime('%H',start_time) AS INTEGER) "
            "BETWEEN 17 AND 20 THEN 1 ELSE 0 END),"
            "SUM(CASE WHEN CAST(strftime('%H',start_time) AS INTEGER) < 7 "
            "OR CAST(strftime('%H',start_time) AS INTEGER) >= 23 THEN 1 ELSE 0 END) "
            "FROM charging_order WHERE legacy_source='formal_seed_v1'"))
        || !peaks.next()) return false;
    return peaks.value(0).toInt() > peaks.value(1).toInt();
}

bool verifyForeignKeys(QSqlDatabase database)
{
    QSqlQuery query(database);
    return query.exec(QStringLiteral("PRAGMA foreign_key_check")) && !query.next();
}

bool verifyStatistics(QSqlDatabase database)
{
    QHash<qint64, QPair<int, qint64>> expected;
    QSqlQuery orders(database);
    if (!orders.exec(QStringLiteral(
            "SELECT charger_id,start_time,end_time FROM charging_order "
            "WHERE legacy_source='formal_seed_v1'"))) return false;
    while (orders.next()) {
        auto &statistics = expected[orders.value(0).toLongLong()];
        ++statistics.first;
        statistics.second += ncs::DateTimeStorage::fromText(orders.value(1).toString())
                                 .secsTo(ncs::DateTimeStorage::fromText(
                                     orders.value(2).toString())) / 60;
    }
    QSqlQuery chargers(database);
    if (!chargers.exec(QStringLiteral(
            "SELECT id,total_count,total_minutes FROM charger"))) return false;
    while (chargers.next()) {
        const auto statistics = expected.value(chargers.value(0).toLongLong());
        if (chargers.value(1).toInt() != statistics.first
            || chargers.value(2).toLongLong() != statistics.second) return false;
    }
    return true;
}

bool verifyAdmin(QSqlDatabase database)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(
            "SELECT password_hash,salt FROM admin WHERE username='admin'"))
        || !query.next()) return false;
    return query.value(0).toString() != QStringLiteral("123456")
        && ncs::PasswordHasher::verify(QStringLiteral("123456"),
                                      query.value(1).toString(),
                                      query.value(0).toString());
}

void printStatistics(QSqlDatabase database)
{
    QTextStream output(stdout);
    auto scalar = [database](const QString &sql) {
        QSqlQuery query(database);
        return query.exec(sql) && query.next() ? query.value(0).toString() : QStringLiteral("?");
    };
    output << "users=" << scalar(QStringLiteral("SELECT COUNT(*) FROM user")) << '\n'
           << "stations=" << scalar(QStringLiteral("SELECT COUNT(*) FROM station")) << '\n'
           << "chargers=" << scalar(QStringLiteral("SELECT COUNT(*) FROM charger")) << '\n'
           << "eligible_chargers=" << scalar(QStringLiteral("SELECT COUNT(*) FROM charger WHERE status=0")) << '\n'
           << "orders=" << scalar(QStringLiteral("SELECT COUNT(*) FROM charging_order")) << '\n'
           << "completed_orders=" << scalar(QStringLiteral("SELECT COUNT(*) FROM charging_order WHERE status=2")) << '\n'
           << "date_min=" << scalar(QStringLiteral("SELECT MIN(date(start_time)) FROM charging_order")) << '\n'
           << "date_max=" << scalar(QStringLiteral("SELECT MAX(date(start_time)) FROM charging_order")) << '\n'
           << "daily_min=" << scalar(QStringLiteral("SELECT MIN(order_count) FROM (SELECT date(start_time), COUNT(*) AS order_count FROM charging_order GROUP BY date(start_time))")) << '\n'
           << "daily_max=" << scalar(QStringLiteral("SELECT MAX(order_count) FROM (SELECT date(start_time), COUNT(*) AS order_count FROM charging_order GROUP BY date(start_time))")) << '\n'
           << "daily_avg=" << scalar(QStringLiteral("SELECT printf('%.2f', AVG(order_count)) FROM (SELECT date(start_time), COUNT(*) AS order_count FROM charging_order GROUP BY date(start_time))")) << '\n'
           << "user_min=" << scalar(QStringLiteral("SELECT MIN(order_count) FROM (SELECT user_id, COUNT(*) AS order_count FROM charging_order GROUP BY user_id)")) << '\n'
           << "user_max=" << scalar(QStringLiteral("SELECT MAX(order_count) FROM (SELECT user_id, COUNT(*) AS order_count FROM charging_order GROUP BY user_id)")) << '\n'
           << "user_avg=" << scalar(QStringLiteral("SELECT printf('%.2f', AVG(order_count)) FROM (SELECT user_id, COUNT(*) AS order_count FROM charging_order GROUP BY user_id)")) << '\n'
           << "station_coverage=" << scalar(QStringLiteral("SELECT COUNT(DISTINCT station_id) FROM charging_order")) << '\n'
           << "charger_coverage=" << scalar(QStringLiteral("SELECT COUNT(DISTINCT charger_id) FROM charging_order")) << '\n'
           << "user_coverage=" << scalar(QStringLiteral("SELECT COUNT(DISTINCT user_id) FROM charging_order")) << '\n';
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::DatabaseManager first(directory.filePath(QStringLiteral("first.db")));
    if (!first.initialize()) return 1;
    if (!countEquals(first.connection(), QStringLiteral("admin"), 1)
        || !countEquals(first.connection(), QStringLiteral("user"), ExpectedUsers)
        || !countEquals(first.connection(), QStringLiteral("station"), ExpectedStations)
        || !countEquals(first.connection(), QStringLiteral("charger"), ExpectedChargers)
        || !countEquals(first.connection(), QStringLiteral("charging_order"), ExpectedOrders)
        || !verifyAdmin(first.connection()) || !verifyUsers(first.connection())
        || !verifyCatalog(first.connection()) || !verifyOrders(first.connection())
        || !verifyDistribution(first.connection()) || !verifyForeignKeys(first.connection())
        || !verifyStatistics(first.connection())) return 2;
    printStatistics(first.connection());
    const QString firstSnapshot = snapshot(first.connection());
    if (firstSnapshot.isEmpty() || !first.initialize()
        || !countEquals(first.connection(), QStringLiteral("user"), ExpectedUsers)
        || !countEquals(first.connection(), QStringLiteral("charging_order"), ExpectedOrders)
        || snapshot(first.connection()) != firstSnapshot) return 3;

    ncs::DatabaseManager second(directory.filePath(QStringLiteral("second.db")));
    if (!second.initialize() || snapshot(second.connection()) != firstSnapshot) return 4;
    return 0;
}
