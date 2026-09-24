#pragma once

#include <QString>

namespace ncs {

bool isValidTimeString(const QString &value);
bool isTimeInWindow(const QString &current, const QString &start, const QString &end);
bool hasEffectiveReminderTime(const QString &reminderStart, const QString &reminderEnd,
                              const QString &dndStart, const QString &dndEnd);

}
