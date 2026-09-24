#include "charger_service.h"

#include "repository/charger_repository.h"

#include <QtMath>
#include <cmath>

namespace ncs {

ServiceResult<ChargerStatusSummary> ChargerService::statusSummary() const
{
    ChargerStatusSummary result;
    QString error;
    if (!repository_.statusSummary(&result, &error)) {
        return ServiceResult<ChargerStatusSummary>::fail(
            BusinessErrorCode::DatabaseError, error);
    }
    if (result.total > 0) {
        const auto percent = [&](qint64 count) {
            return qRound64(10000.0 * count / result.total) / 100.0;
        };
        result.idlePercent = percent(result.idle);
        result.inUsePercent = percent(result.inUse);
        result.faultPercent = percent(result.fault);
        result.health = percent(result.idle + result.inUse);
    }
    return ServiceResult<ChargerStatusSummary>::ok(result);
}

ServiceResult<QVector<Charger>> ChargerService::list(const QString &keyword,
                                                     int status) const
{
    return list(keyword, status, -1);
}

ServiceResult<QVector<Charger>> ChargerService::list(const QString &keyword,
                                                     int status, qint64 stationId) const
{
    if (status < -1 || status > static_cast<int>(ChargerStatus::Fault) || stationId == 0) {
        return ServiceResult<QVector<Charger>>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("电桩状态筛选无效"));
    }
    QVector<Charger> chargers;
    QString error;
    if (!repository_.list(keyword.trimmed(), status, &chargers, &error, stationId)) {
        return ServiceResult<QVector<Charger>>::fail(BusinessErrorCode::DatabaseError, error);
    }
    return ServiceResult<QVector<Charger>>::ok(chargers);
}

ServiceResult<QVector<Charger>> ChargerService::batchCreate(qint64 stationId,
                                                            const QString &rawPrefix,
                                                            int count, int type,
                                                            double powerKw) const
{
    const QString prefix = rawPrefix.trimmed();
    constexpr int kMaxBatchCount = 100;
    if (stationId <= 0 || prefix.isEmpty() || prefix.size() > 48 || count < 1
        || count > kMaxBatchCount || !isValidChargerType(type) || !std::isfinite(powerKw)
        || powerKw <= 0.0) {
        return ServiceResult<QVector<Charger>>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("批量建桩参数无效"));
    }
    bool stationFound = false;
    QString error;
    if (!repository_.stationExists(stationId, &stationFound, &error))
        return ServiceResult<QVector<Charger>>::fail(BusinessErrorCode::DatabaseError, error);
    if (!stationFound)
        return ServiceResult<QVector<Charger>>::fail(BusinessErrorCode::StationNotFound,
                                                     QStringLiteral("充电站不存在"));
    QStringList codes;
    for (int i = 1; i <= count; ++i) codes.append(QStringLiteral("%1-%2").arg(prefix).arg(i, 3, 10, QLatin1Char('0')));
    QVector<Charger> chargers;
    if (!repository_.insertBatch(stationId, codes, type, powerKw, &chargers, &error)) {
        if (error.contains(QStringLiteral("UNIQUE"), Qt::CaseInsensitive))
            return ServiceResult<QVector<Charger>>::fail(BusinessErrorCode::InvalidArgument,
                                                         QStringLiteral("生成的电桩编号已存在"));
        return ServiceResult<QVector<Charger>>::fail(BusinessErrorCode::DatabaseError, error);
    }
    return ServiceResult<QVector<Charger>>::ok(chargers);
}

ServiceResult<Charger> ChargerService::create(qint64 stationId, const QString &rawCode,
                                               int type, double powerKw) const
{
    const QString code = rawCode.trimmed();
    if (stationId <= 0 || code.isEmpty() || code.size() > 64 || !isValidChargerType(type)
        || !std::isfinite(powerKw) || powerKw <= 0.0) {
        return ServiceResult<Charger>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("电桩参数无效"));
    }
    bool stationFound = false;
    QString error;
    if (!repository_.stationExists(stationId, &stationFound, &error)) {
        return ServiceResult<Charger>::fail(BusinessErrorCode::DatabaseError, error);
    }
    if (!stationFound) {
        return ServiceResult<Charger>::fail(
            BusinessErrorCode::StationNotFound, QStringLiteral("充电站不存在"));
    }
    Charger charger;
    if (!repository_.insert(stationId, code, type, powerKw, &charger, &error)) {
        if (error.contains(QStringLiteral("UNIQUE"), Qt::CaseInsensitive)) {
            return ServiceResult<Charger>::fail(
                BusinessErrorCode::InvalidArgument, QStringLiteral("电桩编号已存在"));
        }
        return ServiceResult<Charger>::fail(BusinessErrorCode::DatabaseError, error);
    }
    return ServiceResult<Charger>::ok(charger);
}

ServiceResult<Charger> ChargerService::remove(qint64 id) const
{
    if (id <= 0) {
        return ServiceResult<Charger>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("电桩编号无效"));
    }
    Charger charger;
    bool found = false;
    QString error;
    if (!repository_.findById(id, &charger, &found, &error)) {
        return ServiceResult<Charger>::fail(BusinessErrorCode::DatabaseError, error);
    }
    if (!found) {
        return ServiceResult<Charger>::fail(
            BusinessErrorCode::ChargerNotFound, QStringLiteral("电桩不存在"));
    }
    if (charger.status == ChargerStatus::Using || charger.activeOrderStatus >= 0) {
        return ServiceResult<Charger>::fail(
            BusinessErrorCode::ChargerUnavailable, QStringLiteral("使用中的电桩不能删除"));
    }
    bool removed = false;
    if (!repository_.remove(id, &removed, &error)) {
        return ServiceResult<Charger>::fail(BusinessErrorCode::DatabaseError, error);
    }
    if (!removed) {
        return ServiceResult<Charger>::fail(
            BusinessErrorCode::ChargerNotFound, QStringLiteral("电桩不存在"));
    }
    return ServiceResult<Charger>::ok(charger);
}

ServiceResult<Charger> ChargerService::changeStatus(qint64 id, ChargerStatus expected,
                                                     ChargerStatus next) const
{
    if (id <= 0) {
        return ServiceResult<Charger>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("电桩编号无效"));
    }
    Charger charger;
    bool found = false;
    QString error;
    if (!repository_.findById(id, &charger, &found, &error)) {
        return ServiceResult<Charger>::fail(BusinessErrorCode::DatabaseError, error);
    }
    if (!found) {
        return ServiceResult<Charger>::fail(
            BusinessErrorCode::ChargerNotFound, QStringLiteral("电桩不存在"));
    }
    if (charger.status != expected || charger.activeOrderStatus >= 0) {
        return ServiceResult<Charger>::fail(
            BusinessErrorCode::ChargerUnavailable, QStringLiteral("当前电桩状态不允许此操作"));
    }
    bool updated = false;
    if (!repository_.updateStatus(id, expected, next, &updated, &error)) {
        return ServiceResult<Charger>::fail(BusinessErrorCode::DatabaseError, error);
    }
    if (!updated) {
        return ServiceResult<Charger>::fail(
            BusinessErrorCode::ChargerUnavailable, QStringLiteral("电桩状态已发生变化"));
    }
    charger.status = next;
    return ServiceResult<Charger>::ok(charger);
}

ServiceResult<Charger> ChargerService::markFault(qint64 id) const
{
    return changeStatus(id, ChargerStatus::Idle, ChargerStatus::Fault);
}

ServiceResult<Charger> ChargerService::recover(qint64 id) const
{
    return changeStatus(id, ChargerStatus::Fault, ChargerStatus::Idle);
}

ServiceResult<Charger> ChargerService::restart(qint64 id) const
{
    if (id <= 0) {
        return ServiceResult<Charger>::fail(
            BusinessErrorCode::InvalidArgument, QStringLiteral("电桩编号无效"));
    }
    Charger charger;
    bool found = false;
    QString error;
    if (!repository_.findById(id, &charger, &found, &error)) {
        return ServiceResult<Charger>::fail(BusinessErrorCode::DatabaseError, error);
    }
    if (!found) {
        return ServiceResult<Charger>::fail(
            BusinessErrorCode::ChargerNotFound, QStringLiteral("电桩不存在"));
    }
    if (charger.status == ChargerStatus::Using || charger.activeOrderStatus >= 0) {
        return ServiceResult<Charger>::fail(
            BusinessErrorCode::ChargerUnavailable, QStringLiteral("使用中的电桩不能远程重启"));
    }
    return ServiceResult<Charger>::ok(charger);
}

}
