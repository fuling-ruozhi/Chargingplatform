#pragma once

#include <QMetaType>

namespace ncs {

struct ChargerStatusSummary
{
    qint64 total = 0;
    qint64 idle = 0;
    qint64 inUse = 0;
    qint64 fault = 0;
    double idlePercent = 0.0;
    double inUsePercent = 0.0;
    double faultPercent = 0.0;
    double health = 0.0;
};

}

Q_DECLARE_METATYPE(ncs::ChargerStatusSummary)
