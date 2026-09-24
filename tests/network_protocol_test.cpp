#include "config/network_config.h"
#include "network/frame_codec.h"
#include "network/json_protocol.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtEndian>

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
        qInfo().noquote() << QStringLiteral("Network protocol checks: %1 passed / %2 total")
                                 .arg(passed_)
                                 .arg(total_);
        return passed_ == total_ ? 0 : 1;
    }

private:
    int passed_ = 0;
    int total_ = 0;
};

bool append(ncs::FrameCodec *codec,
            const QByteArray &bytes,
            QList<QByteArray> *frames)
{
    QString error;
    return codec->append(bytes, frames, &error);
}

}  // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    TestRunner tests;
    const QByteArray firstPayload = QByteArrayLiteral("{\"value\":1}");
    const QByteArray secondPayload = QByteArrayLiteral("{\"value\":2}");
    const QByteArray firstFrame = ncs::FrameCodec::encode(firstPayload);
    const QByteArray secondFrame = ncs::FrameCodec::encode(secondPayload);

    ncs::FrameCodec completeCodec;
    QList<QByteArray> frames;
    tests.check(append(&completeCodec, firstFrame, &frames)
                    && frames == QList<QByteArray>{firstPayload},
                QStringLiteral("complete frame is decoded"));

    ncs::FrameCodec splitHeaderCodec;
    frames.clear();
    const bool partialHeader = append(&splitHeaderCodec, firstFrame.left(2), &frames)
                               && frames.isEmpty();
    const bool completedHeader = append(&splitHeaderCodec, firstFrame.mid(2), &frames)
                                 && frames == QList<QByteArray>{firstPayload};
    tests.check(partialHeader && completedHeader,
                QStringLiteral("header split across reads is decoded"));

    ncs::FrameCodec splitPayloadCodec;
    frames.clear();
    const bool partialPayload = append(&splitPayloadCodec, firstFrame.left(7), &frames)
                                && frames.isEmpty();
    const bool completedPayload = append(&splitPayloadCodec, firstFrame.mid(7), &frames)
                                  && frames == QList<QByteArray>{firstPayload};
    tests.check(partialPayload && completedPayload,
                QStringLiteral("payload split across reads is decoded"));

    ncs::FrameCodec stickyCodec;
    frames.clear();
    tests.check(append(&stickyCodec, firstFrame + secondFrame, &frames)
                    && frames == QList<QByteArray>{firstPayload, secondPayload},
                QStringLiteral("two sticky frames are decoded"));

    ncs::FrameCodec oneAndHalfCodec;
    frames.clear();
    const bool firstPart = append(&oneAndHalfCodec,
                                  firstFrame + secondFrame.left(6),
                                  &frames)
                           && frames == QList<QByteArray>{firstPayload};
    frames.clear();
    const bool secondPart = append(&oneAndHalfCodec, secondFrame.mid(6), &frames)
                            && frames == QList<QByteArray>{secondPayload};
    tests.check(firstPart && secondPart,
                QStringLiteral("one-and-a-half frames complete correctly"));

    QByteArray oversizedHeader(4, Qt::Uninitialized);
    qToBigEndian<quint32>(ncs::NetworkConfig::MaximumFramePayload + 1,
                          reinterpret_cast<uchar *>(oversizedHeader.data()));
    ncs::FrameCodec oversizedCodec;
    frames.clear();
    QString oversizedError;
    tests.check(!oversizedCodec.append(oversizedHeader, &frames, &oversizedError)
                    && !oversizedError.isEmpty(),
                QStringLiteral("oversized frame is rejected"));

    const ncs::JsonRequest sourceRequest{
        QStringLiteral("request-1"), QStringLiteral("system.ping"), QJsonObject()};
    ncs::JsonRequest decodedRequest;
    ncs::ProtocolError error;
    tests.check(ncs::JsonProtocol::decodeRequest(
                    ncs::JsonProtocol::encodeRequest(sourceRequest), &decodedRequest, &error)
                    && decodedRequest.requestId == sourceRequest.requestId
                    && decodedRequest.type == sourceRequest.type,
                QStringLiteral("normal JSON request round-trips"));

    QJsonObject responseData;
    responseData.insert(QStringLiteral("pong"), true);
    const ncs::JsonResponse sourceResponse = ncs::JsonProtocol::success(
        QStringLiteral("request-1"), responseData);
    ncs::JsonResponse decodedResponse;
    tests.check(ncs::JsonProtocol::decodeResponse(
                    ncs::JsonProtocol::encodeResponse(sourceResponse), &decodedResponse, &error)
                    && decodedResponse.success
                    && decodedResponse.data.value(QStringLiteral("pong")).toBool(),
                QStringLiteral("normal JSON response round-trips"));

    tests.check(!ncs::JsonProtocol::decodeRequest(
                    QByteArrayLiteral("{broken"), &decodedRequest, &error)
                    && error.code == ncs::ProtocolErrorCode::MalformedJson,
                QStringLiteral("malformed JSON is rejected"));
    tests.check(!ncs::JsonProtocol::decodeRequest(
                    QJsonDocument(QJsonArray()).toJson(QJsonDocument::Compact),
                    &decodedRequest,
                    &error)
                    && error.code == ncs::ProtocolErrorCode::InvalidJsonRoot,
                QStringLiteral("non-object JSON root is rejected"));

    const QByteArray missingType = QByteArrayLiteral(
        "{\"request_id\":\"request-2\",\"data\":{}}");
    tests.check(!ncs::JsonProtocol::decodeRequest(missingType, &decodedRequest, &error)
                    && error.code == ncs::ProtocolErrorCode::MissingType,
                QStringLiteral("missing request type is rejected"));
    const QByteArray missingId = QByteArrayLiteral(
        "{\"type\":\"system.ping\",\"data\":{}}");
    tests.check(!ncs::JsonProtocol::decodeRequest(missingId, &decodedRequest, &error)
                    && error.code == ncs::ProtocolErrorCode::MissingRequestId,
                QStringLiteral("missing request id is rejected"));

    return tests.finish();
}
