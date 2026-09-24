#pragma once

namespace ncs {

class AdminService;
class RequestRouter;
class SessionManager;
void registerLogRoutes(RequestRouter &router, AdminService &adminService,
                       SessionManager &sessionManager);

class AdminRoutes
{
public:
    static void registerAll(RequestRouter &router, AdminService &adminService,
                            SessionManager &sessionManager);
    static void registerPredictionRoutes(RequestRouter &router,
                                         AdminService &adminService,
                                         SessionManager &sessionManager);
};

}
