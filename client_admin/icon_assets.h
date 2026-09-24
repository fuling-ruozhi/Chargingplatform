#pragma once
#include <QIcon>
#include <QPixmap>
namespace ncs::uiicons {
enum class Symbol { Analytics, Status, Charger, Station, Users, Prediction, Refresh, Logout, Help, Search, Empty };
QIcon icon(Symbol symbol, bool selectable = false);
QPixmap trendPixmap();
}

