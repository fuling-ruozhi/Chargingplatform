#include "admin_client_facade.h"

namespace ncs {

void AdminClientFacade::requestChargers(const QString &keyword, int status, qint64 stationId)
{
    if (!isConnected()) {
        emit requestFailed(QStringLiteral("admin.charger.list"), 2001,
                           QStringLiteral("服务器未连接"), 0);
        return;
    }
    QJsonObject data{{QStringLiteral("admin_session_token"), sessionToken_},
                     {QStringLiteral("keyword"), keyword},
                     {QStringLiteral("status"), status}};
    if (stationId > 0) data.insert(QStringLiteral("station_id"), stationId);
    send(QStringLiteral("admin.charger.list"), data);
}

void AdminClientFacade::requestStations(const QString &keyword)
{
    send(QStringLiteral("admin.station.list"), {{QStringLiteral("admin_session_token"), sessionToken_}, {QStringLiteral("keyword"), keyword}});
}

void AdminClientFacade::createStation(const Station &station)
{
    send(QStringLiteral("admin.station.create"), {{QStringLiteral("admin_session_token"), sessionToken_},
        {QStringLiteral("name"), station.name}, {QStringLiteral("address"), station.address},
        {QStringLiteral("longitude"), station.longitude}, {QStringLiteral("latitude"), station.latitude},
        {QStringLiteral("price"), station.price}, {QStringLiteral("total_slots"), station.totalSlots}});
}

void AdminClientFacade::updateStation(const Station &station)
{
    QJsonObject data{{QStringLiteral("admin_session_token"), sessionToken_}, {QStringLiteral("station_id"), station.id},
        {QStringLiteral("name"), station.name}, {QStringLiteral("address"), station.address},
        {QStringLiteral("longitude"), station.longitude}, {QStringLiteral("latitude"), station.latitude},
        {QStringLiteral("price"), station.price}, {QStringLiteral("total_slots"), station.totalSlots}};
    send(QStringLiteral("admin.station.update"), data);
}

void AdminClientFacade::deleteStation(qint64 stationId)
{
    send(QStringLiteral("admin.station.delete"), {{QStringLiteral("admin_session_token"), sessionToken_}, {QStringLiteral("station_id"), stationId}});
}

void AdminClientFacade::batchCreateChargers(qint64 stationId, const QString &prefix, int count,
                                            int type, double powerKw)
{
    send(QStringLiteral("admin.station.batchCreateChargers"), {{QStringLiteral("admin_session_token"), sessionToken_},
        {QStringLiteral("station_id"), stationId}, {QStringLiteral("prefix"), prefix},
        {QStringLiteral("count"), count}, {QStringLiteral("type"), type}, {QStringLiteral("power_kw"), powerKw}});
}

void AdminClientFacade::createCharger(qint64 stationId, const QString &code,
                                      int type, double powerKw)
{
    send(QStringLiteral("admin.charger.create"),
         {{QStringLiteral("admin_session_token"), sessionToken_},
          {QStringLiteral("station_id"), stationId}, {QStringLiteral("code"), code},
          {QStringLiteral("type"), type}, {QStringLiteral("power_kw"), powerKw}});
}

void AdminClientFacade::deleteCharger(qint64 chargerId)
{
    send(QStringLiteral("admin.charger.delete"),
         {{QStringLiteral("admin_session_token"), sessionToken_},
          {QStringLiteral("charger_id"), chargerId}});
}

void AdminClientFacade::markChargerFault(qint64 chargerId)
{
    send(QStringLiteral("admin.charger.markFault"),
         {{QStringLiteral("admin_session_token"), sessionToken_},
          {QStringLiteral("charger_id"), chargerId}});
}

void AdminClientFacade::recoverCharger(qint64 chargerId)
{
    send(QStringLiteral("admin.charger.recover"),
         {{QStringLiteral("admin_session_token"), sessionToken_},
          {QStringLiteral("charger_id"), chargerId}});
}

void AdminClientFacade::restartCharger(qint64 chargerId)
{
    send(QStringLiteral("admin.charger.restart"),
         {{QStringLiteral("admin_session_token"), sessionToken_},
          {QStringLiteral("charger_id"), chargerId}});
}

}  // namespace ncs
