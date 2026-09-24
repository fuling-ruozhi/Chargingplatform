#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMetaType>
#include <QString>

namespace ncs {

enum class ProtocolErrorCode {
    Ok = 0,
    MalformedJson = 1001,
    InvalidJsonRoot = 1002,
    MissingRequestId = 1003,
    MissingType = 1004,
    InvalidData = 1005,
    UnknownRequestType = 1006,
    FrameTooLarge = 1007,
    NetworkError = 2001
};

struct JsonRequest
{
    QString requestId;
    QString type;
    QJsonObject data;
};

struct JsonResponse
{
    QString requestId;
    bool success = false;
    int code = static_cast<int>(ProtocolErrorCode::Ok);
    QString message;
    QJsonObject data;
};

struct ProtocolError
{
    QString requestId;
    ProtocolErrorCode code = ProtocolErrorCode::Ok;
    QString message;
};

class JsonProtocol
{
public:
    static QByteArray encodeRequest(const JsonRequest &request);
    static bool decodeRequest(const QByteArray &payload,
                              JsonRequest *request,
                              ProtocolError *error);

    static QByteArray encodeResponse(const JsonResponse &response);
    static bool decodeResponse(const QByteArray &payload,
                               JsonResponse *response,
                               ProtocolError *error);

    static JsonResponse success(const QString &requestId,
                                const QJsonObject &data = QJsonObject(),
                                const QString &message = QStringLiteral("ok"));
    static JsonResponse failure(const QString &requestId,
                                ProtocolErrorCode code,
                                const QString &message,
                                const QJsonObject &data = QJsonObject());
    static JsonResponse failure(const QString &requestId,
                                int code,
                                const QString &message,
                                const QJsonObject &data = QJsonObject());
};

}  // namespace ncs

Q_DECLARE_METATYPE(ncs::JsonRequest)
Q_DECLARE_METATYPE(ncs::JsonResponse)
