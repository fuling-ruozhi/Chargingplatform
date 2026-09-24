#include "admin_route_validation.h"
#include <cmath>
namespace ncs {
namespace {
bool number(const QJsonObject &data, const QString &key) { return data.value(key).isDouble() && std::isfinite(data.value(key).toDouble()); }
bool fail(QString *parameter, QString *reason, const QString &p, const QString &r) { *parameter = p; *reason = r; return false; }
}
bool validAdminLoginInput(const QJsonObject &data, QString *p, QString *r)
{
    if (!data.value("username").isString() || data.value("username").toString().trimmed().isEmpty()) return fail(p,r,"username","expected non-empty string");
    if (!data.value("password").isString() || data.value("password").toString().isEmpty()) return fail(p,r,"password","expected non-empty string");
    return true;
}
bool validAdminChargerInput(const QJsonObject &data, QString *p, QString *r)
{
    const auto station = data.value("station_id"); const auto type = data.value("type"); const auto power = data.value("power_kw");
    if (!station.isDouble() || station.toDouble() <= 0 || station.toDouble() != station.toInteger()) return fail(p,r,"station_id","expected positive integer");
    if (!type.isDouble() || type.toInt(-1) < 0 || type.toInt(-1) > 1) return fail(p,r,"type","invalid enum");
    if (!power.isDouble() || !std::isfinite(power.toDouble()) || power.toDouble() <= 0) return fail(p,r,"power_kw","out of range");
    if (!data.value("code").isString() || data.value("code").toString().trimmed().isEmpty()) return fail(p,r,"code","expected non-empty string");
    return true;
}
bool validAdminStationInput(const QJsonObject &data, QString *p, QString *r)
{
    if (!data.value("name").isString() || data.value("name").toString().trimmed().isEmpty()) return fail(p,r,"name","expected non-empty string");
    if (!data.value("address").isString() || data.value("address").toString().trimmed().isEmpty()) return fail(p,r,"address","expected non-empty string");
    if (!number(data,"longitude") || data.value("longitude").toDouble() < -180 || data.value("longitude").toDouble() > 180) return fail(p,r,"longitude","out of range");
    if (!number(data,"latitude") || data.value("latitude").toDouble() < -90 || data.value("latitude").toDouble() > 90) return fail(p,r,"latitude","out of range");
    if (!number(data,"price") || data.value("price").toDouble() <= 0) return fail(p,r,"price","out of range");
    if (!data.value("total_slots").isDouble() || data.value("total_slots").toInt(-1) < 0) return fail(p,r,"total_slots","out of range");
    return true;
}
bool validAdminBatchChargerInput(const QJsonObject &data, QString *p, QString *r)
{
    const auto station = data.value("station_id"); const auto count = data.value("count");
    if (!station.isDouble() || station.toDouble() <= 0 || station.toDouble() != station.toInteger()) return fail(p,r,"station_id","expected positive integer");
    if (!count.isDouble() || count.toDouble() != count.toInteger() || count.toInt() <= 0 || count.toInt() > 100) return fail(p,r,"count","out of range");
    if (!data.value("prefix").isString() || data.value("prefix").toString().trimmed().isEmpty()) return fail(p,r,"prefix","expected non-empty string");
    const auto type = data.value("type"); const auto power = data.value("power_kw");
    if (!type.isUndefined() && (!type.isDouble() || type.toInt(-1) < 0 || type.toInt(-1) > 1)) return fail(p,r,"type","invalid enum");
    if (!power.isUndefined() && (!power.isDouble() || !std::isfinite(power.toDouble()) || power.toDouble() <= 0)) return fail(p,r,"power_kw","out of range");
    return true;
}
}
