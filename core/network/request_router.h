#pragma once

#include "json_protocol.h"

#include <QHash>

#include <functional>

namespace ncs {

class RequestRouter
{
public:
    using Handler = std::function<JsonResponse(const JsonRequest &)>;

    RequestRouter();

    void registerHandler(const QString &type, Handler handler);
    JsonResponse route(const JsonRequest &request) const;

private:
    QHash<QString, Handler> handlers_;
};

}  // namespace ncs
