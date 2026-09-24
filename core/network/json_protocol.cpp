#include "json_protocol.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace ncs {
namespace {

bool parseObject(const QByteArray &payload, QJsonObject *object, ProtocolError *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        error->code = ProtocolErrorCode::MalformedJson;
        error->message = QStringLiteral("malformed JSON: %1").arg(parseError.errorString());
        return false;
    }
    if (!document.isObject()) {
        error->code = ProtocolErrorCode::InvalidJsonRoot;
        error->message = QStringLiteral("JSON root must be an object");
        return false;
    }
    *object = document.object();
    return true;
}

bool readRequiredString(const QJsonObject &object,
                        const QString &name,
                        QString *value,
                        ProtocolErrorCode code,
                        ProtocolError *error)
{
    const QJsonValue field = object.value(name);
    if (!field.isString() || field.toString().isEmpty()) {
        error->code = code;
        error->message = QStringLiteral("missing or invalid %1").arg(name);
        return false;
    }
    *value = field.toString();
    return true;
}

}  // namespace

QByteArray JsonProtocol::encodeRequest(const JsonRequest &request)
{
    QJsonObject object;
    object.insert(QStringLiteral("request_id"), request.requestId);
    object.insert(QStringLiteral("type"), request.type);
    object.insert(QStringLiteral("data"), request.data);
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

bool JsonProtocol::decodeRequest(const QByteArray &payload,
                                 JsonRequest *request,
                                 ProtocolError *error)
{
    *error = ProtocolError();
    QJsonObject object;
    if (!parseObject(payload, &object, error)) {
        return false;
    }

    const QJsonValue requestIdValue = object.value(QStringLiteral("request_id"));
    if (requestIdValue.isString()) {
        error->requestId = requestIdValue.toString();
    }
    if (!readRequiredString(object,
                            QStringLiteral("request_id"),
                            &request->requestId,
                            ProtocolErrorCode::MissingRequestId,
                            error)
        || !readRequiredString(object,
                               QStringLiteral("type"),
                               &request->type,
                               ProtocolErrorCode::MissingType,
                               error)) {
        return false;
    }

    const QJsonValue data = object.value(QStringLiteral("data"));
    if (!data.isUndefined() && !data.isObject()) {
        error->requestId = request->requestId;
        error->code = ProtocolErrorCode::InvalidData;
        error->message = QStringLiteral("data must be an object");
        return false;
    }
    request->data = data.isObject() ? data.toObject() : QJsonObject();
    return true;
}

QByteArray JsonProtocol::encodeResponse(const JsonResponse &response)
{
    QJsonObject object;
    object.insert(QStringLiteral("request_id"), response.requestId);
    object.insert(QStringLiteral("success"), response.success);
    object.insert(QStringLiteral("code"), response.code);
    object.insert(QStringLiteral("message"), response.message);
    object.insert(QStringLiteral("data"), response.data);
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

bool JsonProtocol::decodeResponse(const QByteArray &payload,
                                  JsonResponse *response,
                                  ProtocolError *error)
{
    *error = ProtocolError();
    QJsonObject object;
    if (!parseObject(payload, &object, error)) {
        return false;
    }
    const QJsonValue requestId = object.value(QStringLiteral("request_id"));
    if (!requestId.isString()) {
        error->code = ProtocolErrorCode::MissingRequestId;
        error->message = QStringLiteral("missing or invalid request_id");
        return false;
    }
    response->requestId = requestId.toString();
    if (!object.value(QStringLiteral("success")).isBool()
        || !object.value(QStringLiteral("code")).isDouble()
        || !object.value(QStringLiteral("message")).isString()) {
        error->requestId = response->requestId;
        error->code = ProtocolErrorCode::InvalidData;
        error->message = QStringLiteral("response fields have invalid types");
        return false;
    }
    const QJsonValue data = object.value(QStringLiteral("data"));
    if (!data.isUndefined() && !data.isObject()) {
        error->requestId = response->requestId;
        error->code = ProtocolErrorCode::InvalidData;
        error->message = QStringLiteral("response data must be an object");
        return false;
    }

    response->success = object.value(QStringLiteral("success")).toBool();
    response->code = object.value(QStringLiteral("code")).toInt();
    response->message = object.value(QStringLiteral("message")).toString();
    response->data = data.isObject() ? data.toObject() : QJsonObject();
    return true;
}

JsonResponse JsonProtocol::success(const QString &requestId,
                                   const QJsonObject &data,
                                   const QString &message)
{
    return {requestId, true, static_cast<int>(ProtocolErrorCode::Ok), message, data};
}

JsonResponse JsonProtocol::failure(const QString &requestId,
                                   ProtocolErrorCode code,
                                   const QString &message,
                                   const QJsonObject &data)
{
    return {requestId, false, static_cast<int>(code), message, data};
}

JsonResponse JsonProtocol::failure(const QString &requestId,
                                   int code,
                                   const QString &message,
                                   const QJsonObject &data)
{
    return {requestId, false, code, message, data};
}

}  // namespace ncs
