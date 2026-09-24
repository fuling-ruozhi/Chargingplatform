#pragma once

#include "json_protocol.h"

namespace ncs {

class SessionManager;

bool resolveUser(const JsonRequest &request, SessionManager &sessions,
                 qint64 *userId, JsonResponse *failure);
bool shouldEchoOtpForDemo(const JsonRequest &request);

}
