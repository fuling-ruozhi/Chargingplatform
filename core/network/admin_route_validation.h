#pragma once
#include <QJsonObject>
#include <QString>
namespace ncs {
bool validAdminLoginInput(const QJsonObject &, QString *parameter, QString *reason);
bool validAdminChargerInput(const QJsonObject &, QString *parameter, QString *reason);
bool validAdminStationInput(const QJsonObject &, QString *parameter, QString *reason);
bool validAdminBatchChargerInput(const QJsonObject &, QString *parameter, QString *reason);
}
