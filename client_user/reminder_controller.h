#pragma once

#include "model/user_preference.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>

namespace ncs {

class UserClientFacade;

class ReminderEdgeState
{
public:
    QVector<ReminderMatch> update(const QVector<ReminderMatch> &matches,
                                  const QDateTime &now);
    void reset();

private:
    QSet<qint64> activeStations_;
    QHash<qint64, QDateTime> lastReminderTimes_;
};

class ReminderController : public QObject
{
    Q_OBJECT

public:
    explicit ReminderController(UserClientFacade &facade,
                                 QObject *parent = nullptr);
    void setEnabled(bool enabled);
    bool enabled() const { return enabled_; }
    bool timerActive() const { return timer_.isActive(); }
    void start();
    void stop();
    bool requestPending() const { return requestPending_; }

signals:
    void notificationRequested(const QVector<ncs::ReminderMatch> &matches);

private:
    void requestNow();

    UserClientFacade &facade_;
    QTimer timer_;
    ReminderEdgeState edgeState_;
    bool enabled_ = false;
    bool requestPending_ = false;
};

}  // namespace ncs
