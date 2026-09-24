#pragma once

namespace ncs {

class RequestRouter;
class SessionManager;
class UserPreferenceService;
class ReminderService;

class PreferenceRoutes
{
public:
    static void registerAll(RequestRouter &, SessionManager &, UserPreferenceService &,
                            ReminderService &);
};

}
