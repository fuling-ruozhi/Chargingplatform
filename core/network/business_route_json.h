#pragma once

#include "model/user.h"
#include "model/user_preference.h"
#include "model/vehicle_profile.h"

#include <QJsonObject>

namespace ncs {

QJsonObject userJson(const User &user);
QJsonObject legacyUserJson(const User &user);
QJsonObject userPreferenceJson(const UserPreference &preference);
QJsonObject reminderMatchJson(const ReminderMatch &match);
QJsonObject vehicleProfileJson(const VehicleProfile &profile);

}
