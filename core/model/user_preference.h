#pragma once

#include <QDateTime>
#include <QVector>
#include <QString>

namespace ncs {

struct UserPreference
{
    qint64 userId = 0;
    bool hasHomeLocation = false;
    bool homeLatitudeSet = false;
    bool homeLongitudeSet = false;
    double homeLatitude = 0.0;
    double homeLongitude = 0.0;
    double homeRadiusKm = 3.0;
    QVector<int> preferredChargerTypes;
    QString reminderStartTime = QStringLiteral("08:00");
    QString reminderEndTime = QStringLiteral("22:00");
    int minIdleChargers = 1;
    QString dndStartTime = QStringLiteral("22:00");
    QString dndEndTime = QStringLiteral("07:00");
    bool enabled = true;
    QString updatedAt;
};

struct ReminderMatch
{
    qint64 stationId = 0;
    QString stationName;
    double distanceKm = 0.0;
    bool hasDistance = false;
    QVector<int> matchedTypes;
    int idleMatchedChargers = 0;
    bool favorite = false;
    bool insideHomeRadius = false;
    QString reason;
};

}

Q_DECLARE_METATYPE(ncs::UserPreference)
Q_DECLARE_METATYPE(ncs::ReminderMatch)
Q_DECLARE_METATYPE(QVector<ncs::ReminderMatch>)
