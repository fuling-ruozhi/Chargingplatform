#include "model/business_error.h"
#include "network/network_client.h"
#include "network/network_server_host.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QThread>

#include <functional>

namespace {

bool waitUntil(const std::function<bool()> &condition)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1);
    }
    return condition();
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::NetworkServerHost server(directory.filePath(QStringLiteral("auth.db")));
    quint16 port = 0;
    QObject::connect(&server, &ncs::NetworkServerHost::serverStarted,
                     [&](quint16 value) { port = value; });
    server.startServer(QStringLiteral("127.0.0.1"), 0);
    if (!waitUntil([&] { return port > 0; })) return 1;

    ncs::NetworkClient client;
    bool connected = false;
    QList<ncs::JsonResponse> responses;
    QObject::connect(&client, &ncs::NetworkClient::connected,
                     [&] { connected = true; });
    QObject::connect(&client, &ncs::NetworkClient::responseReceived,
                     [&](const ncs::JsonResponse &response) { responses.append(response); });
    client.connectToServer(QStringLiteral("127.0.0.1"), port);
    if (!waitUntil([&] { return connected; })) return 2;

    auto call = [&](const QString &id, const QString &route, const QJsonObject &data) {
        client.sendRequest(route, data, id);
        waitUntil([&] {
            for (const auto &response : responses) {
                if (response.requestId == id) return true;
            }
            return false;
        });
        for (const auto &response : responses) {
            if (response.requestId == id) return response;
        }
        return ncs::JsonResponse();
    };

    const auto missing = call(QStringLiteral("missing"),
                              QStringLiteral("user.profile.get"), {});
    if (missing.success || missing.code
        != static_cast<int>(ncs::BusinessErrorCode::AuthRequired)) return 3;
    const auto otpA = call(QStringLiteral("otp-a"), QStringLiteral("user.otp.request"),
                           {{QStringLiteral("phone"), QStringLiteral("13800138001")}});
    const auto loginA = call(QStringLiteral("login-a"), QStringLiteral("user.login"),
        {{QStringLiteral("phone"), QStringLiteral("13800138001")},
         {QStringLiteral("code"), otpA.data.value(QStringLiteral("display_code"))}});
    const QString tokenA = loginA.data.value(QStringLiteral("session_token")).toString();
    const QJsonObject userA = loginA.data.value(QStringLiteral("user")).toObject();
    if (!otpA.success || !loginA.success || tokenA.size() != 64
        || userA.value(QStringLiteral("phone_masked")).toString()
            != QStringLiteral("138****8001")) return 4;

    const auto otpB = call(QStringLiteral("otp-b"), QStringLiteral("user.otp.request"),
                           {{QStringLiteral("phone"), QStringLiteral("13800138002")}});
    const auto loginB = call(QStringLiteral("login-b"), QStringLiteral("user.login"),
        {{QStringLiteral("phone"), QStringLiteral("13800138002")},
         {QStringLiteral("code"), otpB.data.value(QStringLiteral("display_code"))}});
    const QString tokenB = loginB.data.value(QStringLiteral("session_token")).toString();
    if (!loginB.success || tokenB == tokenA) return 5;

    const auto defaultPreference = call(QStringLiteral("preference-get-a"),
                                        QStringLiteral("preference.get"),
                                        {{QStringLiteral("session_token"), tokenA}});
    if (!defaultPreference.success
        || defaultPreference.data.value(QStringLiteral("home_latitude")).isDouble()
        || defaultPreference.data.value(QStringLiteral("home_radius_km")).toDouble() != 3.0)
        return 10;
    const auto updatedPreference = call(QStringLiteral("preference-update-a"),
        QStringLiteral("preference.update"),
        {{QStringLiteral("session_token"), tokenA},
         {QStringLiteral("home_latitude"), 39.9623},
         {QStringLiteral("home_longitude"), 116.3220},
         {QStringLiteral("home_radius_km"), 2.0},
         {QStringLiteral("preferred_charger_types"), QJsonArray{1}},
         {QStringLiteral("reminder_start_time"), QStringLiteral("08:00")},
         {QStringLiteral("reminder_end_time"), QStringLiteral("22:00")},
         {QStringLiteral("min_idle_chargers"), 1},
         {QStringLiteral("dnd_start_time"), QStringLiteral("22:00")},
         {QStringLiteral("dnd_end_time"), QStringLiteral("07:00")},
         {QStringLiteral("enabled"), true}});
    if (!updatedPreference.success
        || updatedPreference.data.value(QStringLiteral("home_radius_km")).toDouble() != 2.0)
        return 11;
    const auto favorite = call(QStringLiteral("favorite-add-a"),
        QStringLiteral("favorite_station.add"),
        {{QStringLiteral("session_token"), tokenA}, {QStringLiteral("station_id"), 1}});
    const auto favorites = call(QStringLiteral("favorite-list-a"),
        QStringLiteral("favorite_station.list"),
        {{QStringLiteral("session_token"), tokenA}});
    const auto otherFavorites = call(QStringLiteral("favorite-list-b"),
        QStringLiteral("favorite_station.list"),
        {{QStringLiteral("session_token"), tokenB}});
    if (!favorite.success || !favorites.success || !otherFavorites.success
        || favorites.data.value(QStringLiteral("station_ids")).toArray().size() != 1
        || !otherFavorites.data.value(QStringLiteral("station_ids")).toArray().isEmpty())
        return 12;
    const auto reminder = call(QStringLiteral("reminder-check-a"),
        QStringLiteral("reminder.check"),
        {{QStringLiteral("session_token"), tokenA}});
    if (!reminder.success || !reminder.data.value(QStringLiteral("alerts")).isArray()) return 13;

    const auto forged = call(QStringLiteral("forged"),
        QStringLiteral("user.profile.nickname.update"),
        {{QStringLiteral("session_token"), tokenA},
         {QStringLiteral("user_id"), userA.value(QStringLiteral("id")).toInteger() + 1},
         {QStringLiteral("nickname"), QStringLiteral("A用户")}});
    const auto profileA = call(QStringLiteral("profile-a"),
        QStringLiteral("user.profile.get"),
        {{QStringLiteral("session_token"), tokenA}});
    const auto profileB = call(QStringLiteral("profile-b"),
        QStringLiteral("user.profile.get"),
        {{QStringLiteral("session_token"), tokenB}});
    if (!forged.success || profileA.data.value(QStringLiteral("nickname")).toString()
            != QStringLiteral("A用户")
        || profileB.data.value(QStringLiteral("nickname")).toString()
            == QStringLiteral("A用户")) return 6;

    const auto recharge = call(QStringLiteral("recharge"), QStringLiteral("user.recharge"),
        {{QStringLiteral("session_token"), tokenA}, {QStringLiteral("amount"), 25.50}});
    if (!recharge.success
        || recharge.data.value(QStringLiteral("balance_after")).toDouble() != 25.50)
        return 7;
    if (!call(QStringLiteral("logout"), QStringLiteral("user.logout"),
              {{QStringLiteral("session_token"), tokenA}}).success) return 8;
    const auto expired = call(QStringLiteral("expired"),
        QStringLiteral("user.profile.get"),
        {{QStringLiteral("session_token"), tokenA}});
    if (expired.success || expired.code
        != static_cast<int>(ncs::BusinessErrorCode::SessionExpired)) return 9;

    client.disconnectFromServer();
    server.shutdown();
    return 0;
}
