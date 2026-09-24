#include "service/auth_rate_limiter.h"
#include "model/business_error.h"

#include <QCoreApplication>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    qint64 clockMs = 0;
    ncs::AuthRateLimiter limiter([&clockMs] { return clockMs; });

    if (!limiter.allowOtpRequest(QStringLiteral("13800138001"))) return 1;
    if (limiter.allowOtpRequest(QStringLiteral("13800138001"))) return 2;
    if (!limiter.allowOtpRequest(QStringLiteral("13800138002"))) return 3;
    clockMs += 60 * 1000;
    if (!limiter.allowOtpRequest(QStringLiteral("13800138001"))) return 4;
    clockMs += 60 * 1000;
    if (!limiter.allowOtpRequest(QStringLiteral("13800138001"))) return 5;
    clockMs += 60 * 1000;
    if (!limiter.allowOtpRequest(QStringLiteral("13800138001"))) return 6;
    clockMs += 60 * 1000;
    if (!limiter.allowOtpRequest(QStringLiteral("13800138001"))) return 7;
    clockMs += 60 * 1000;
    if (limiter.allowOtpRequest(QStringLiteral("13800138001"))) return 8;
    clockMs += 10 * 60 * 1000;
    if (!limiter.allowOtpRequest(QStringLiteral("13800138001"))) return 9;

    if (!limiter.allowLogin(QStringLiteral("user-a"))) return 10;
    for (int attempt = 0; attempt < 4; ++attempt) {
        if (limiter.recordLoginFailure(QStringLiteral("user-a"))) return 11;
    }
    if (!limiter.allowLogin(QStringLiteral("user-a"))) return 12;
    if (!limiter.recordLoginFailure(QStringLiteral("user-a"))) return 13;
    if (limiter.allowLogin(QStringLiteral("user-a"))) return 14;
    if (!limiter.allowLogin(QStringLiteral("user-b"))) return 15;
    limiter.recordLoginSuccess(QStringLiteral("user-b"));
    clockMs += 60 * 1000;
    if (!limiter.allowLogin(QStringLiteral("user-a"))) return 16;

    return 0;
}
