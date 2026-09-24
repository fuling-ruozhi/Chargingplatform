#include "network/json_protocol.h"
#include "network/network_client.h"
#include "network/network_server_host.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QList>
#include <QThread>
#include <QTemporaryDir>

#include <functional>

namespace {

class TestRunner
{
public:
    void check(bool condition, const QString &description)
    {
        ++total_;
        if (condition) {
            ++passed_;
            qInfo().noquote() << QStringLiteral("PASS: %1").arg(description);
        } else {
            qCritical().noquote() << QStringLiteral("FAIL: %1").arg(description);
        }
    }

    int finish() const
    {
        qInfo().noquote() << QStringLiteral("Network integration checks: %1 passed / %2 total")
                                 .arg(passed_)
                                 .arg(total_);
        return passed_ == total_ ? 0 : 1;
    }

private:
    int passed_ = 0;
    int total_ = 0;
};

bool waitUntil(const std::function<bool()> &condition, int timeoutMilliseconds = 3000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMilliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1);
    }
    return condition();
}

const ncs::JsonResponse *findResponse(const QList<ncs::JsonResponse> &responses,
                                      const QString &requestId)
{
    for (const ncs::JsonResponse &response : responses) {
        if (response.requestId == requestId) {
            return &response;
        }
    }
    return nullptr;
}

}  // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    TestRunner tests;

    QTemporaryDir temporaryDirectory;
    ncs::NetworkServerHost server(
        temporaryDirectory.filePath(QStringLiteral("phase2-network-test.db")));
    quint16 listeningPort = 0;
    QString serverError;
    QObject::connect(&server, &ncs::NetworkServerHost::serverStarted,
                     [&listeningPort](quint16 port) { listeningPort = port; });
    QObject::connect(&server, &ncs::NetworkServerHost::serverError,
                     [&serverError](const QString &message) { serverError = message; });
    server.startServer(QStringLiteral("127.0.0.1"), 0);
    tests.check(waitUntil([&listeningPort, &serverError]() {
                    return listeningPort != 0 || !serverError.isEmpty();
                }) && listeningPort != 0,
                QStringLiteral("server listens on an ephemeral loopback port"));

    ncs::NetworkClient client;
    bool connected = false;
    bool disconnected = false;
    QString clientError;
    QList<ncs::JsonResponse> responses;
    QObject::connect(&client, &ncs::NetworkClient::connected,
                     [&connected]() { connected = true; });
    QObject::connect(&client, &ncs::NetworkClient::disconnected,
                     [&disconnected]() { disconnected = true; });
    QObject::connect(&client, &ncs::NetworkClient::networkError,
                     [&clientError](const QString &message) { clientError = message; });
    QObject::connect(&client, &ncs::NetworkClient::responseReceived,
                     [&responses](const ncs::JsonResponse &response) {
                         responses.append(response);
                     });

    client.connectToServer(QStringLiteral("127.0.0.1"), listeningPort);
    tests.check(waitUntil([&connected, &clientError]() {
                    return connected || !clientError.isEmpty();
                }) && connected,
                QStringLiteral("client connects to loopback server"));

    const QString pingId = QStringLiteral("integration-ping-1");
    tests.check(client.sendRequest(QStringLiteral("system.ping"), QJsonObject(), pingId)
                    == pingId,
                QStringLiteral("system.ping request is sent"));
    tests.check(waitUntil([&responses, &pingId]() {
                    return findResponse(responses, pingId) != nullptr;
                }),
                QStringLiteral("system.ping receives a response"));
    const ncs::JsonResponse *pingResponse = findResponse(responses, pingId);
    tests.check(pingResponse && pingResponse->requestId == pingId,
                QStringLiteral("ping response preserves request_id"));
    tests.check(pingResponse && pingResponse->success && pingResponse->code == 0,
                QStringLiteral("ping response reports success"));
    tests.check(pingResponse
                    && pingResponse->data.value(QStringLiteral("pong")).toBool()
                    && pingResponse->data.value(QStringLiteral("protocol_version")).toInt() == 1,
                QStringLiteral("ping response contains pong and protocol version"));

    const QString unknownId = QStringLiteral("integration-unknown-1");
    tests.check(client.sendRequest(QStringLiteral("unknown.operation"),
                                   QJsonObject(), unknownId) == unknownId,
                QStringLiteral("unknown request is sent"));
    tests.check(waitUntil([&responses, &unknownId]() {
                    return findResponse(responses, unknownId) != nullptr;
                }),
                QStringLiteral("unknown request receives a response"));
    const ncs::JsonResponse *unknownResponse = findResponse(responses, unknownId);
    tests.check(unknownResponse && unknownResponse->requestId == unknownId
                    && !unknownResponse->success && unknownResponse->code != 0
                    && unknownResponse->message == QStringLiteral("unknown request type"),
                QStringLiteral("unknown request returns the unified error response"));

    client.disconnectFromServer();
    tests.check(waitUntil([&disconnected]() { return disconnected; }),
                QStringLiteral("client disconnects cleanly"));
    server.shutdown();
    return tests.finish();
}
