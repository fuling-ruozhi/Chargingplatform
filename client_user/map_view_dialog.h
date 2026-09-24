#pragma once

#include <QDialog>

class QWebEngineView;

namespace ncs {

// UC-U-04 增强：内嵌腾讯地图（QtWebEngine）。仅在 NCS_HAS_WEBENGINE 时编译。
// Key 从环境变量 NCS_TENCENT_MAP_KEY 读取，未配置时按钮侧不应打开本窗口。
class MapViewDialog : public QDialog
{
    Q_OBJECT

public:
    MapViewDialog(double stationLongitude, double stationLatitude,
                  const QString &stationName, const QString &stationAddress,
                  double distanceKm, QWidget *parent = nullptr,
                  bool hasUserPosition = false, double userLongitude = 0.0,
                  double userLatitude = 0.0);
    ~MapViewDialog() override;

private:
    QWebEngineView *view_ = nullptr;
    QString htmlPath_;
    double longitude_ = 0.0;
    double latitude_ = 0.0;
    QString name_;
    QString address_;
    double distanceKm_ = 0.0;
    bool hasUserPosition_ = false;
    double userLongitude_ = 0.0;
    double userLatitude_ = 0.0;
};

}  // namespace ncs
