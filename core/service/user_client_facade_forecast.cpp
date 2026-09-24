// EXT-SC-03 续航预测请求（NFR-M-01：与 user_client_facade.cpp 拆分）
#include "user_client_facade.h"

#include "model/charge_forecast.h"

namespace ncs {

void UserClientFacade::requestChargeForecast()
{
    sendAuthenticated(QStringLiteral("user.forecast"));
}

void UserClientFacade::handleForecastResponse(const JsonResponse &response)
{
    ChargeForecast forecast;
    forecast.charging = response.data.value(QStringLiteral("charging")).toBool();
    forecast.coldStart = response.data.value(QStringLiteral("cold_start")).toBool();
    forecast.currentSoc =
        response.data.value(QStringLiteral("current_soc")).toDouble();
    forecast.dailyKwh = response.data.value(QStringLiteral("daily_kwh")).toDouble();
    forecast.remainingDays =
        response.data.value(QStringLiteral("remaining_days")).toDouble();
    forecast.suggestedDate =
        response.data.value(QStringLiteral("suggested_date")).toString();
    forecast.method = response.data.value(QStringLiteral("method")).toString();
    forecast.note = response.data.value(QStringLiteral("note")).toString();
    emit chargeForecastReceived(forecast);
}

}
