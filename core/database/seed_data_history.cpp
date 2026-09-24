#include "seed_data_history.h"

#include "util/charge_calculator.h"
#include "util/date_time_storage.h"

#include <QDateTime>
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>
#include <QVector>
#include <QVariant>

#include <algorithm>
#include <climits>

namespace ncs {
namespace {

const QString SeedSource = QStringLiteral("formal_seed_v1");

struct DemoUserSeed
{
    qint64 id = 0;
    double initialBalance = 0.0;
    double balance = 0.0;
    int orderCount = 0;
};

bool queryError(const QSqlQuery &query, const QString &context, QString *error)
{
    *error = QStringLiteral("%1: %2").arg(context, query.lastError().text());
    return false;
}

bool insertDemoUsers(QSqlDatabase &database, QList<DemoUserSeed> *users,
                     QString *error)
{
    QSqlQuery query(database);
    if (!query.prepare(QStringLiteral(
            "INSERT INTO user(phone,nickname,avatar_path,balance,status,created_at,username) "
            "VALUES(:phone,:nickname,'',:balance,1,:created_at,:username)"))) {
        return queryError(query, QStringLiteral("Cannot prepare demo user seed"), error);
    }
    constexpr int DemoUserCount = 40;
    for (int index = 0; index < DemoUserCount; ++index) {
        const QString phone = index == 0
            ? QStringLiteral("13900000000")
            : QStringLiteral("100%1").arg(index, 8, 10, QLatin1Char('0'));
        const QString username = index == 0
            ? QStringLiteral("formal-seed-user")
            : QStringLiteral("demo-seed-user-%1")
                  .arg(index, 3, 10, QLatin1Char('0'));
        const double balance = 8000.0 + index * 200.0;
        query.bindValue(QStringLiteral(":phone"), phone);
        query.bindValue(QStringLiteral(":nickname"), index == 0
                            ? QStringLiteral("演示用户")
                            : QStringLiteral("模拟用户%1")
                                  .arg(index, 2, 10, QLatin1Char('0')));
        query.bindValue(QStringLiteral(":balance"), balance);
        query.bindValue(QStringLiteral(":created_at"),
                        QStringLiteral("2026-08-01 00:00:00.000"));
        query.bindValue(QStringLiteral(":username"), username);
        if (!query.exec()) {
            return queryError(query, QStringLiteral("Cannot seed demo user"), error);
        }
        users->append({query.lastInsertId().toLongLong(), balance, balance, 0});
    }
    return true;
}

QTime generatedStartTime(QRandomGenerator *random, bool weekend)
{
    const int roll = static_cast<int>(random->bounded(100u));
    int startMinute = 0;
    int spanMinutes = 0;
    if (weekend) {
        if (roll < 10) {
            startMinute = 8 * 60;
            spanMinutes = 120;
        } else if (roll < 32) {
            startMinute = 10 * 60;
            spanMinutes = 240;
        } else if (roll < 62) {
            startMinute = 14 * 60;
            spanMinutes = 240;
        } else if (roll < 90) {
            startMinute = 18 * 60;
            spanMinutes = 240;
        } else {
            startMinute = 22 * 60;
            spanMinutes = 60;
        }
    } else if (roll < 28) {
        startMinute = 7 * 60;
        spanMinutes = 150;
    } else if (roll < 40) {
        startMinute = 9 * 60 + 30;
        spanMinutes = 120;
    } else if (roll < 55) {
        startMinute = 11 * 60 + 30;
        spanMinutes = 150;
    } else if (roll < 68) {
        startMinute = 14 * 60;
        spanMinutes = 180;
    } else if (roll < 92) {
        startMinute = 17 * 60;
        spanMinutes = 240;
    } else {
        startMinute = 21 * 60;
        spanMinutes = 120;
    }
    const int minute = startMinute + static_cast<int>(random->bounded(
        static_cast<quint32>(spanMinutes)));
    return QTime(minute / 60, minute % 60);
}

template <typename T>
void deterministicShuffle(QVector<T> *values, QRandomGenerator *random)
{
    for (int index = values->size() - 1; index > 0; --index) {
        const int other = static_cast<int>(random->bounded(
            static_cast<quint32>(index + 1)));
        std::swap((*values)[index], (*values)[other]);
    }
}

QVector<int> buildDailyOrderCounts(QRandomGenerator *random)
{
    constexpr int DayCount = 30;
    constexpr int TargetTotal = 1200;
    QVector<int> counts;
    counts.reserve(DayCount);
    int total = 0;
    for (int day = 0; day < DayCount; ++day) {
        const QDate date(2026, 8, 2);
        const bool weekend = date.addDays(day).dayOfWeek() >= 6;
        const int minimum = weekend ? 35 : 39;
        const int count = minimum + static_cast<int>(random->bounded(6u));
        counts.append(count);
        total += count;
    }
    int cursor = 0;
    while (total < TargetTotal) {
        ++counts[cursor % DayCount];
        ++total;
        cursor += 7;
    }
    cursor = 0;
    while (total > TargetTotal) {
        const int index = cursor % DayCount;
        const QDate date(2026, 8, 2);
        const int minimum = date.addDays(index).dayOfWeek() >= 6 ? 35 : 39;
        if (counts[index] > minimum) {
            --counts[index];
            --total;
        }
        ++cursor;
    }
    return counts;
}

bool generateOrders(QList<DemoUserSeed> *users,
                    const QList<ChargerSeed> &chargers,
                    QList<QVariantMap> *orders, QString *error)
{
    struct CandidateOrder {
        QDateTime proposedStart;
        int durationMinutes = 0;
        int userIndex = 0;
        double initialSoc = 0.0;
    };
    QRandomGenerator random(20260904u);
    QRandomGenerator countRandom(20260905u);
    QRandomGenerator coverageRandom(20260906u);
    const QDate firstDate(2026, 8, 2);
    const QVector<int> dailyCounts = buildDailyOrderCounts(&countRandom);
    QVector<int> userOrderCounts(users->size(), 0);
    QVector<int> userCoverage(users->size());
    for (int index = 0; index < userCoverage.size(); ++index) userCoverage[index] = index;
    deterministicShuffle(&userCoverage, &coverageRandom);
    QVector<int> chargerCoverage(chargers.size());
    for (int index = 0; index < chargerCoverage.size(); ++index) chargerCoverage[index] = index;
    deterministicShuffle(&chargerCoverage, &coverageRandom);
    QVector<QDateTime> occupiedUntil(chargers.size());
    QVector<int> chargerOrderCounts(chargers.size(), 0);
    int generatedOrderCount = 0;
    int coveredChargers = 0;
    for (int day = 0; day < dailyCounts.size(); ++day) {
        const QDate date = firstDate.addDays(day);
        const bool weekend = date.dayOfWeek() >= 6;
        QList<CandidateOrder> candidates;
        for (int index = 0; index < dailyCounts.at(day); ++index) {
            int userIndex = 0;
            if (generatedOrderCount < userCoverage.size()) {
                userIndex = userCoverage.at(generatedOrderCount);
            } else {
                int minimum = INT_MAX;
                for (const int count : userOrderCounts) minimum = qMin(minimum, count);
                QVector<int> eligibleUsers;
                for (int candidate = 0; candidate < userOrderCounts.size(); ++candidate) {
                    if (userOrderCounts.at(candidate) <= minimum + 4) eligibleUsers.append(candidate);
                }
                userIndex = eligibleUsers.at(static_cast<int>(random.bounded(
                    static_cast<quint32>(eligibleUsers.size()))));
            }
            ++userOrderCounts[userIndex];
            ++generatedOrderCount;
            candidates.append({QDateTime(date, generatedStartTime(&random, weekend)),
                               20 + static_cast<int>(random.bounded(36u)), userIndex,
                               15.0 + static_cast<double>(random.bounded(71u))});
        }
        std::sort(candidates.begin(), candidates.end(),
                  [](const CandidateOrder &left, const CandidateOrder &right) {
                      return left.proposedStart < right.proposedStart;
                  });
        for (const CandidateOrder &candidate : candidates) {
            QDateTime start = candidate.proposedStart;
            QVector<int> eligibleChargers;
            for (int charger = 0; charger < chargers.size(); ++charger) {
                if (!occupiedUntil.at(charger).isValid()
                    || occupiedUntil.at(charger) <= start) eligibleChargers.append(charger);
            }
            int chargerIndex = -1;
            if (coveredChargers < chargerCoverage.size()) {
                const int preferred = chargerCoverage.at(coveredChargers);
                if (eligibleChargers.contains(preferred)) {
                    chargerIndex = preferred;
                    ++coveredChargers;
                }
            }
            if (chargerIndex < 0 && !eligibleChargers.isEmpty()) {
                int minimum = INT_MAX;
                for (const int item : eligibleChargers) {
                    minimum = qMin(minimum, chargerOrderCounts.at(item));
                }
                QVector<int> leastUsed;
                for (const int item : eligibleChargers) {
                    if (chargerOrderCounts.at(item) == minimum) leastUsed.append(item);
                }
                chargerIndex = leastUsed.at(static_cast<int>(random.bounded(
                    static_cast<quint32>(leastUsed.size()))));
            }
            if (chargerIndex < 0) {
                QDateTime earliest;
                for (int charger = 0; charger < occupiedUntil.size(); ++charger) {
                    if (!earliest.isValid() || occupiedUntil.at(charger) < earliest) {
                        earliest = occupiedUntil.at(charger);
                        chargerIndex = charger;
                    }
                }
                start = earliest;
            }
            const QDateTime end = start.addSecs(candidate.durationMinutes * 60);
            const ChargerSeed &charger = chargers.at(chargerIndex);
            const ChargeMetrics metrics = ChargeCalculator::calculate(
                start, end, charger.powerKw, charger.pricePerKwh, 1,
                candidate.initialSoc, 60.0);
            (*users)[candidate.userIndex].balance -= metrics.amount;
            if ((*users)[candidate.userIndex].balance < 0.0) {
                *error = QStringLiteral("Formal seed history exceeds demo balance");
                return false;
            }
            ++(*users)[candidate.userIndex].orderCount;
            ++chargerOrderCounts[chargerIndex];
            occupiedUntil[chargerIndex] = end;
            QVariantMap order;
            order.insert(QStringLiteral("user_index"), candidate.userIndex);
            order.insert(QStringLiteral("charger_index"), chargerIndex);
            order.insert(QStringLiteral("start"), start);
            order.insert(QStringLiteral("end"), end);
            order.insert(QStringLiteral("initial_soc"), candidate.initialSoc);
            order.insert(QStringLiteral("energy"), metrics.energyKwh);
            order.insert(QStringLiteral("amount"), metrics.amount);
            order.insert(QStringLiteral("final_soc"), metrics.soc);
            orders->append(order);
        }
    }
    return true;
}

bool insertOrders(QSqlDatabase &database, QList<DemoUserSeed> *users,
                  const QList<ChargerSeed> &chargers,
                  const QList<QVariantMap> &orders, QString *error)
{
    QSqlQuery insert(database);
    if (!insert.prepare(QStringLiteral(
            "INSERT INTO charging_order(order_no,user_id,charger_id,station_id,status,"
            "reserved_at,expire_at,start_time,end_time,energy,amount,debt_amount,"
            "balance_after,price_per_kwh,power_kw,time_scale,initial_soc,final_soc,"
            "station_name_snapshot,charger_code_snapshot,created_at,updated_at,"
            "legacy_source,legacy_id) VALUES(:order_no,:user_id,:charger_id,:station_id,2,"
            ":reserved_at,:expire_at,:start_time,:end_time,:energy,:amount,0,"
            ":balance_after,:price,:power,1,:initial_soc,:final_soc,:station_name,"
            ":charger_code,:created_at,:updated_at,:source,:legacy_id)"))) {
        return queryError(insert, QStringLiteral("Cannot prepare order history seed"), error);
    }
    QSqlQuery updateStats(database);
    if (!updateStats.prepare(QStringLiteral(
            "UPDATE charger SET total_count=total_count+1,"
            "total_minutes=total_minutes+:minutes WHERE id=:charger_id"))) {
        return queryError(updateStats, QStringLiteral("Cannot prepare seed statistics"), error);
    }
    QVector<double> runningBalances;
    for (const DemoUserSeed &user : *users) runningBalances.append(user.initialBalance);
    for (int index = 0; index < orders.size(); ++index) {
        const QVariantMap &order = orders.at(index);
        const int userIndex = order.value(QStringLiteral("user_index")).toInt();
        const ChargerSeed &charger = chargers.at(
            order.value(QStringLiteral("charger_index")).toInt());
        const QDateTime startTime = order.value(QStringLiteral("start")).toDateTime();
        const QDateTime endTime = order.value(QStringLiteral("end")).toDateTime();
        double &balance = runningBalances[userIndex];
        balance -= order.value(QStringLiteral("amount")).toDouble();
        const QString start = DateTimeStorage::toText(startTime);
        const QString end = DateTimeStorage::toText(endTime);
        insert.bindValue(QStringLiteral(":order_no"),
                         QStringLiteral("SEED-%1").arg(index + 1, 4, 10, QLatin1Char('0')));
        insert.bindValue(QStringLiteral(":user_id"), users->at(userIndex).id);
        insert.bindValue(QStringLiteral(":charger_id"), charger.id);
        insert.bindValue(QStringLiteral(":station_id"), charger.stationId);
        insert.bindValue(QStringLiteral(":reserved_at"), DateTimeStorage::toText(startTime.addSecs(-300)));
        insert.bindValue(QStringLiteral(":expire_at"), DateTimeStorage::toText(startTime.addSecs(600)));
        insert.bindValue(QStringLiteral(":start_time"), start);
        insert.bindValue(QStringLiteral(":end_time"), end);
        insert.bindValue(QStringLiteral(":energy"), order.value(QStringLiteral("energy")));
        insert.bindValue(QStringLiteral(":amount"), order.value(QStringLiteral("amount")));
        insert.bindValue(QStringLiteral(":balance_after"), balance);
        insert.bindValue(QStringLiteral(":price"), charger.pricePerKwh);
        insert.bindValue(QStringLiteral(":power"), charger.powerKw);
        insert.bindValue(QStringLiteral(":initial_soc"), order.value(QStringLiteral("initial_soc")));
        insert.bindValue(QStringLiteral(":final_soc"), order.value(QStringLiteral("final_soc")));
        insert.bindValue(QStringLiteral(":station_name"), charger.stationName);
        insert.bindValue(QStringLiteral(":charger_code"), charger.code);
        insert.bindValue(QStringLiteral(":created_at"), DateTimeStorage::toText(startTime.addSecs(-300)));
        insert.bindValue(QStringLiteral(":updated_at"), end);
        insert.bindValue(QStringLiteral(":source"), SeedSource);
        insert.bindValue(QStringLiteral(":legacy_id"), index + 1);
        if (!insert.exec()) return queryError(insert, QStringLiteral("Cannot seed order history"), error);
        updateStats.bindValue(QStringLiteral(":minutes"), startTime.secsTo(endTime) / 60);
        updateStats.bindValue(QStringLiteral(":charger_id"), charger.id);
        if (!updateStats.exec() || updateStats.numRowsAffected() != 1) {
            return queryError(updateStats, QStringLiteral("Cannot seed charger statistics"), error);
        }
    }
    QSqlQuery updateUsers(database);
    if (!updateUsers.prepare(QStringLiteral("UPDATE user SET balance=:balance WHERE id=:id"))) {
        return queryError(updateUsers, QStringLiteral("Cannot prepare demo balance update"), error);
    }
    for (const DemoUserSeed &user : *users) {
        updateUsers.bindValue(QStringLiteral(":balance"), user.balance);
        updateUsers.bindValue(QStringLiteral(":id"), user.id);
        if (!updateUsers.exec() || updateUsers.numRowsAffected() != 1) {
            return queryError(updateUsers, QStringLiteral("Cannot update demo balance"), error);
        }
    }
    return true;
}

}  // namespace

bool seedDemoHistory(QSqlDatabase &database,
                     const QList<ChargerSeed> &chargers, QString *error)
{
    QList<DemoUserSeed> users;
    QList<QVariantMap> orders;
    return insertDemoUsers(database, &users, error)
        && generateOrders(&users, chargers, &orders, error)
        && insertOrders(database, &users, chargers, orders, error);
}

}  // namespace ncs
