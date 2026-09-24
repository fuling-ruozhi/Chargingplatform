#pragma once

#include <QMetaType>
#include <QString>

namespace ncs {

struct Admin
{
    qint64 id = 0;
    QString username;
    QString createdAt;
};

struct AdminSummary
{
    QString databasePath;
    int onlineChargers = 0;
    int totalChargers = 0;
};

}

Q_DECLARE_METATYPE(ncs::Admin)
Q_DECLARE_METATYPE(ncs::AdminSummary)
