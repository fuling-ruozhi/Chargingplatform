#include "reminder_controller.h"

#include "service/user_client_facade.h"

#include <QDateTime>

namespace ncs {

QVector<ReminderMatch> ReminderEdgeState::update(
    const QVector<ReminderMatch> &matches, const QDateTime &now)
{
    constexpr qint64 CooldownMs = 10 * 60 * 1000;
    QSet<qint64> currentStations;
    QVector<ReminderMatch> notifications;
    for (const ReminderMatch &match : matches) {
        currentStations.insert(match.stationId);
        if (activeStations_.contains(match.stationId)) continue;
        const QDateTime previous = lastReminderTimes_.value(match.stationId);
        if (!previous.isValid() || previous.msecsTo(now) >= CooldownMs) {
            notifications.append(match);
            lastReminderTimes_.insert(match.stationId, now);
        }
        activeStations_.insert(match.stationId);
    }

    for (auto it = activeStations_.begin(); it != activeStations_.end();) {
        if (currentStations.contains(*it)) ++it;
        else it = activeStations_.erase(it);
    }
    return notifications;
}

void ReminderEdgeState::reset()
{
    activeStations_.clear();
    lastReminderTimes_.clear();
}

ReminderController::ReminderController(UserClientFacade &facade, QObject *parent)
    : QObject(parent), facade_(facade)
{
    timer_.setInterval(60 * 1000);
    connect(&timer_, &QTimer::timeout, this, &ReminderController::requestNow);
    connect(&facade_, &UserClientFacade::disconnected,
            this, &ReminderController::stop);
    connect(&facade_, &UserClientFacade::sessionInvalid,
            this, &ReminderController::stop);
    connect(&facade_, &UserClientFacade::logoutSucceeded,
            this, &ReminderController::stop);
    connect(&facade_, &UserClientFacade::remindersReceived, this,
            [this](const QVector<ReminderMatch> &matches) {
                requestPending_ = false;
                const auto notifications = edgeState_.update(
                    matches, QDateTime::currentDateTime());
                if (!notifications.isEmpty()) emit notificationRequested(notifications);
            });
    connect(&facade_, &UserClientFacade::requestFailed, this,
            [this](const QString &route, int code, const QString &) {
                if (route != QStringLiteral("reminder.check")) return;
                requestPending_ = false;
                Q_UNUSED(code)
            });
    connect(&facade_, &UserClientFacade::networkError, this,
            [this](const QString &) { requestPending_ = false; });
}

void ReminderController::setEnabled(bool enabled)
{
    enabled_ = enabled;
    if (enabled_) start();
    else stop();
}

void ReminderController::start()
{
    if (!enabled_ || timer_.isActive()) return;
    timer_.start();
    requestNow();
}

void ReminderController::stop()
{
    timer_.stop();
    requestPending_ = false;
    edgeState_.reset();
}

void ReminderController::requestNow()
{
    if (!enabled_ || requestPending_) return;
    requestPending_ = true;
    facade_.checkReminders();
}

}  // namespace ncs
