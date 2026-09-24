#pragma once

#include <QString>
#include <QtGlobal>

namespace ncs::NetworkConfig {

inline QString defaultHost()
{
    return QStringLiteral("127.0.0.1");
}

inline constexpr quint16 DefaultPort = 9527;
inline constexpr quint32 MaximumFramePayload = 1024U * 1024U;
inline constexpr int ProtocolVersion = 1;

}  // namespace ncs::NetworkConfig
