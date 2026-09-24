#include "time_window.h"

namespace ncs {
namespace {

int minuteOfDay(const QString &value)
{
    return value.left(2).toInt() * 60 + value.mid(3, 2).toInt();
}

}

bool isValidTimeString(const QString &value)
{
    if (value.size() != 5 || value.at(2) != QLatin1Char(':')) return false;
    bool okHour = false;
    bool okMinute = false;
    const int hour = value.left(2).toInt(&okHour);
    const int minute = value.mid(3, 2).toInt(&okMinute);
    return okHour && okMinute && hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59
        && value.at(0).isDigit() && value.at(1).isDigit()
        && value.at(3).isDigit() && value.at(4).isDigit();
}

bool isTimeInWindow(const QString &current, const QString &start, const QString &end)
{
    if (!isValidTimeString(current) || !isValidTimeString(start) || !isValidTimeString(end)) {
        return false;
    }
    const int now = minuteOfDay(current);
    const int from = minuteOfDay(start);
    const int to = minuteOfDay(end);
    if (from == to) return true;
    if (from < to) return now >= from && now < to;
    return now >= from || now < to;
}

bool hasEffectiveReminderTime(const QString &reminderStart, const QString &reminderEnd,
                              const QString &dndStart, const QString &dndEnd)
{
    for (int minute = 0; minute < 24 * 60; ++minute) {
        const QString current = QStringLiteral("%1:%2")
            .arg(minute / 60, 2, 10, QLatin1Char('0'))
            .arg(minute % 60, 2, 10, QLatin1Char('0'));
        if (isTimeInWindow(current, reminderStart, reminderEnd)
            && !isTimeInWindow(current, dndStart, dndEnd)) return true;
    }
    return false;
}

}
