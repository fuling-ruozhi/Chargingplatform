#pragma once

#include <QUrl>

namespace ncs {

class NavigationUrl
{
public:
    static QUrl build(double fromLongitude, double fromLatitude,
                      double toLongitude, double toLatitude,
                      const QString &destinationName);
};

}
