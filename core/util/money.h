#pragma once

#include <cmath>
#include <QtGlobal>

namespace ncs {

struct Money
{
    static qint64 toCents(double value)
    {
        return qRound64(value * 100.0);
    }

    static double fromCents(qint64 cents)
    {
        return cents / 100.0;
    }

    static double round(double value)
    {
        return fromCents(toCents(value));
    }

    static bool lessThan(double left, double right)
    {
        return toCents(left) < toCents(right);
    }

    static bool hasCentPrecision(double value)
    {
        return std::fabs(value - fromCents(toCents(value))) < 0.0000001;
    }

    static double subtractFloorZero(double left, double right)
    {
        return fromCents(qMax<qint64>(0, toCents(left) - toCents(right)));
    }

    static double deficit(double required, double available)
    {
        return fromCents(qMax<qint64>(0, toCents(required) - toCents(available)));
    }
};

}
