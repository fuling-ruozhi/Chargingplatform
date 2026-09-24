#include "charger.h"

namespace ncs {

bool isValidChargerType(int type)
{
    return type == static_cast<int>(ChargerType::Slow)
        || type == static_cast<int>(ChargerType::Fast);
}

QString chargerTypeText(int type)
{
    switch (static_cast<ChargerType>(type)) {
    case ChargerType::Slow: return QStringLiteral("交流慢充");
    case ChargerType::Fast: return QStringLiteral("直流快充");
    }
    return QStringLiteral("未知类型");
}

QString chargerStatusText(ChargerStatus status)
{
    switch (status) {
    case ChargerStatus::Idle: return QStringLiteral("空闲");
    case ChargerStatus::Using: return QStringLiteral("使用中");
    case ChargerStatus::Fault: return QStringLiteral("故障");
    }
    return QStringLiteral("未知");
}

}
