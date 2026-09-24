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
    QElapsedTimer timer; timer.start();
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
    ncs::NetworkServerHost server(directory.filePath(QStringLiteral("admin-user.db")));
    quint16 port = 0;
    QObject::connect(&server, &ncs::NetworkServerHost::serverStarted,
                     [&](quint16 value) { port = value; });
    server.startServer(QStringLiteral("127.0.0.1"), 0);
    if (!waitUntil([&] { return port > 0; })) return 1;
    ncs::NetworkClient client;
    bool connected = false;
    QList<ncs::JsonResponse> responses;
    QObject::connect(&client, &ncs::NetworkClient::connected, [&] { connected = true; });
    QObject::connect(&client, &ncs::NetworkClient::responseReceived,
                     [&](const ncs::JsonResponse &response) { responses.append(response); });
    client.connectToServer(QStringLiteral("127.0.0.1"), port);
    if (!waitUntil([&] { return connected; })) return 2;
    auto call = [&](const QString &id, const QString &route, const QJsonObject &data) {
        client.sendRequest(route, data, id);
        if (!waitUntil([&] { for (const auto &response : responses)
                                if (response.requestId == id) return true; return false; }))
            return ncs::JsonResponse();
        for (const auto &response : responses)
            if (response.requestId == id) return response;
        return ncs::JsonResponse();
    };
    const auto login = call(QStringLiteral("admin-login"), QStringLiteral("admin.login"),
        {{QStringLiteral("username"), QStringLiteral("admin")}, {QStringLiteral("password"), QStringLiteral("123456")} });
    const QString adminToken = login.data.value(QStringLiteral("admin_session_token")).toString();
    if (!login.success || adminToken.isEmpty()) return 3;
    const auto list = call(QStringLiteral("user-list"), QStringLiteral("admin.user.list"),
        {{QStringLiteral("admin_session_token"), adminToken}});
    if (!list.success || list.data.value(QStringLiteral("users")).toArray().isEmpty()) return 4;
    const auto search = call(QStringLiteral("user-search"), QStringLiteral("admin.user.list"),
        {{QStringLiteral("admin_session_token"), adminToken}, {QStringLiteral("keyword"), QStringLiteral("formal-seed-user")} });
    if (!search.success || search.data.value(QStringLiteral("users")).toArray().size() != 1) return 5;
    const qint64 userId = search.data.value(QStringLiteral("users")).toArray().first().toObject().value(QStringLiteral("id")).toInteger();
    const QString phone = QStringLiteral("13900000000");
    if (!call(QStringLiteral("freeze"), QStringLiteral("admin.user.freeze"),
              {{QStringLiteral("admin_session_token"), adminToken}, {QStringLiteral("user_id"), userId}}).success) return 6;
    const auto otp = call(QStringLiteral("frozen-otp"), QStringLiteral("user.otp.request"),
        {{QStringLiteral("phone"), phone}, {QStringLiteral("demo_otp_echo"), true}});
    const auto frozenLogin = call(QStringLiteral("frozen-login"), QStringLiteral("user.login"),
        {{QStringLiteral("phone"), phone}, {QStringLiteral("code"), otp.data.value(QStringLiteral("display_code"))}});
    if (!otp.success || frozenLogin.success || frozenLogin.code != static_cast<int>(ncs::BusinessErrorCode::UserFrozen)) return 7;
    if (!call(QStringLiteral("unfreeze"), QStringLiteral("admin.user.unfreeze"),
              {{QStringLiteral("admin_session_token"), adminToken}, {QStringLiteral("user_id"), userId}}).success) return 8;
    const auto restoredLogin = call(QStringLiteral("restored-login"), QStringLiteral("user.login"),
        {{QStringLiteral("phone"), phone}, {QStringLiteral("code"), otp.data.value(QStringLiteral("display_code"))}});
    if (!restoredLogin.success) return 9;
    const auto orders = call(QStringLiteral("user-orders"), QStringLiteral("admin.user.orders"),
        {{QStringLiteral("admin_session_token"), adminToken}, {QStringLiteral("user_id"), userId}});
    if (!orders.success || orders.data.value(QStringLiteral("user_id")).toInteger() != userId
        || !orders.data.value(QStringLiteral("orders")).isArray()) return 10;
    const auto missing = call(QStringLiteral("missing-orders"), QStringLiteral("admin.user.orders"),
        {{QStringLiteral("admin_session_token"), adminToken}, {QStringLiteral("user_id"), 999999}});
    if (missing.success || missing.code != static_cast<int>(ncs::BusinessErrorCode::UserNotFound)) return 11;
    client.disconnectFromServer(); server.shutdown();
    return 0;
}
