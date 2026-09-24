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
    ncs::NetworkServerHost server(
        directory.filePath(QStringLiteral("admin-tcp.db")));
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
                     [&](const ncs::JsonResponse &value) { responses.append(value); });
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
        QStringLiteral("admin.summary"), {});
    if (missing.success || missing.code
        != static_cast<int>(ncs::BusinessErrorCode::AuthRequired)) return 3;
    const auto login = call(QStringLiteral("login"), QStringLiteral("admin.login"),
        {{QStringLiteral("username"), QStringLiteral("admin")},
         {QStringLiteral("password"), QStringLiteral("123456")}});
    const QString token = login.data.value(
        QStringLiteral("admin_session_token")).toString();
    if (!login.success || token.size() != 64
        || login.data.value(QStringLiteral("admin")).toObject()
            .value(QStringLiteral("username")).toString() != QStringLiteral("admin")) return 4;
    const auto summary = call(QStringLiteral("summary"),
        QStringLiteral("admin.summary"),
        {{QStringLiteral("admin_session_token"), token}});
    if (!summary.success
        || summary.data.value(QStringLiteral("total_chargers")).toInt() != 40) return 5;

    const auto revenue = call(QStringLiteral("revenue-summary"),
        QStringLiteral("admin.revenue.summary"),
        {{QStringLiteral("admin_session_token"), token}});
    if (!revenue.success
        || revenue.data.value(QStringLiteral("todayRevenue")).toDouble() < 0.0) return 11;
    const auto invalidTrend = call(QStringLiteral("revenue-trend-invalid"),
        QStringLiteral("admin.revenue.trend"),
        {{QStringLiteral("admin_session_token"), token}, {QStringLiteral("days"), 6}});
    if (invalidTrend.success
        || invalidTrend.code != static_cast<int>(ncs::BusinessErrorCode::InvalidArgument)) {
        return 15;
    }
    const auto trend = call(QStringLiteral("revenue-trend"),
        QStringLiteral("admin.revenue.trend"),
        {{QStringLiteral("admin_session_token"), token}, {QStringLiteral("days"), 7}});
    if (!trend.success || trend.data.value(QStringLiteral("days")).toInt() != 7
        || trend.data.value(QStringLiteral("items")).toArray().size() != 7) return 12;
    const auto recent = call(QStringLiteral("revenue-recent"),
        QStringLiteral("admin.revenue.recentOrders"),
        {{QStringLiteral("admin_session_token"), token}});
    if (!recent.success || recent.data.value(QStringLiteral("items")).toArray().size() > 10) return 13;
    const auto status = call(QStringLiteral("charger-status"),
        QStringLiteral("admin.charger.statusSummary"),
        {{QStringLiteral("admin_session_token"), token}});
    if (!status.success || status.data.value(QStringLiteral("total")).toInt() != 40
        || status.data.value(QStringLiteral("idle")).toInt()
             + status.data.value(QStringLiteral("in_use")).toInt()
             + status.data.value(QStringLiteral("fault")).toInt() != 40) return 14;

    const auto chargerList = call(QStringLiteral("charger-list"),
        QStringLiteral("admin.charger.list"),
        {{QStringLiteral("admin_session_token"), token},
         {QStringLiteral("keyword"), QStringLiteral("NCS-01")},
         {QStringLiteral("status"), -1}});
    if (!chargerList.success || chargerList.requestId != QStringLiteral("charger-list")
        || chargerList.data.value(QStringLiteral("items")).toArray().isEmpty()) return 16;
    const auto createdCharger = call(QStringLiteral("charger-create"),
        QStringLiteral("admin.charger.create"),
        {{QStringLiteral("admin_session_token"), token},
         {QStringLiteral("station_id"), 1},
         {QStringLiteral("code"), QStringLiteral("A05-TCP")},
         {QStringLiteral("type"), 0},
         {QStringLiteral("power_kw"), 60.0}});
    if (!createdCharger.success) return 17;
    const qint64 createdId = createdCharger.data.value(QStringLiteral("charger"))
                                 .toObject().value(QStringLiteral("id")).toInteger();
    if (createdId <= 0) return 18;
    const auto fault = call(QStringLiteral("charger-fault"),
        QStringLiteral("admin.charger.markFault"),
        {{QStringLiteral("admin_session_token"), token},
         {QStringLiteral("charger_id"), createdId}});
    if (!fault.success || fault.requestId != QStringLiteral("charger-fault")) return 19;
    const auto recover = call(QStringLiteral("charger-recover"),
        QStringLiteral("admin.charger.recover"),
        {{QStringLiteral("admin_session_token"), token},
         {QStringLiteral("charger_id"), createdId}});
    if (!recover.success) return 20;
    const auto restart = call(QStringLiteral("charger-restart"),
        QStringLiteral("admin.charger.restart"),
        {{QStringLiteral("admin_session_token"), token},
         {QStringLiteral("charger_id"), createdId}});
    if (!restart.success) return 21;
    const auto deleted = call(QStringLiteral("charger-delete"),
        QStringLiteral("admin.charger.delete"),
        {{QStringLiteral("admin_session_token"), token},
         {QStringLiteral("charger_id"), createdId}});
    if (!deleted.success) return 22;

    const auto otp = call(QStringLiteral("otp"), QStringLiteral("user.otp.request"),
        {{QStringLiteral("phone"), QStringLiteral("13800138009")}});
    const auto userLogin = call(QStringLiteral("user-login"), QStringLiteral("user.login"),
        {{QStringLiteral("phone"), QStringLiteral("13800138009")},
         {QStringLiteral("code"), otp.data.value(QStringLiteral("display_code"))}});
    const QString userToken = userLogin.data.value(QStringLiteral("session_token")).toString();
    const auto wrongTokenType = call(QStringLiteral("wrong-type"),
        QStringLiteral("admin.summary"),
        {{QStringLiteral("admin_session_token"), userToken}});
    if (wrongTokenType.success || wrongTokenType.code
        != static_cast<int>(ncs::BusinessErrorCode::SessionExpired)) return 6;

    if (!call(QStringLiteral("logout"), QStringLiteral("admin.logout"),
              {{QStringLiteral("admin_session_token"), token}}).success) return 7;
    const auto expired = call(QStringLiteral("expired"),
        QStringLiteral("admin.summary"),
        {{QStringLiteral("admin_session_token"), token}});
    if (expired.success || expired.code
        != static_cast<int>(ncs::BusinessErrorCode::SessionExpired)) return 8;

    for (int attempt = 1; attempt <= 5; ++attempt) {
        const auto failed = call(QStringLiteral("bad-%1").arg(attempt),
            QStringLiteral("admin.login"),
            {{QStringLiteral("username"), QStringLiteral("admin")},
             {QStringLiteral("password"), QStringLiteral("wrong")}});
        if (failed.success) return 9;
        if (attempt == 5
            && (failed.code != static_cast<int>(ncs::BusinessErrorCode::AdminLocked)
                || failed.data.value(QStringLiteral("retry_after_seconds")).toInt() <= 0))
            return 10;
    }
    client.disconnectFromServer();
    server.shutdown();
    return 0;
}
