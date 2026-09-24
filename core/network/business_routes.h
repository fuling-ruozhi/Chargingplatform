#pragma once

namespace ncs {

class RequestRouter;
class ChargeService;
class ReviewService;
class UserPreferenceService;
class ReminderService;
class ChargeForecastService;
class StationService;
class UserService;
class SessionManager;

class BusinessRoutes
{
public:
    static void registerAll(RequestRouter &router, UserService &userService,
                                 StationService &stationService, ChargeService &chargeService,
                                 SessionManager &sessionManager,
                                 ReviewService &reviewService,
                                 UserPreferenceService &preferenceService,
                                 ReminderService &reminderService,
                                 ChargeForecastService &forecastService);
private:
    static void registerForecastRoute(RequestRouter &router,
                                      ChargeForecastService &forecastService,
                                      SessionManager &sessionManager);
    static void registerStationRecommendation(RequestRouter &router,
                                              StationService &stationService,
                                              SessionManager &sessionManager);
};

}
