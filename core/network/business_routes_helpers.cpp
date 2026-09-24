#include "business_routes_helpers.h"

#include "service/session_manager.h"

namespace ncs {

bool resolveUser(const JsonRequest &request, SessionManager &sessions,
                 qint64 *userId, JsonResponse *failure)
{
    BusinessErrorCode code = BusinessErrorCode::AuthRequired;
    QString message;
    if (sessions.resolveUser(
            request.data.value(QStringLiteral("session_token")).toString(),
            userId, &code, &message)) return true;
    *failure = JsonProtocol::failure(request.requestId, static_cast<int>(code), message);
    return false;
}

bool shouldEchoOtpForDemo(const JsonRequest &request)
{
    return request.data.value(QStringLiteral("demo_otp_echo")).toBool()
        || qEnvironmentVariableIntValue("NCS_DEMO_OTP_ECHO") == 1;
}

}
