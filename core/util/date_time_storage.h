#pragma once

#include <QDateTime>
#include <QString>

namespace ncs {

class DateTimeStorage
{
public:
    static QDateTime now();
    static QString nowText();
    static QString toText(const QDateTime &value);
    static QDateTime fromText(const QString &value);
};

}
