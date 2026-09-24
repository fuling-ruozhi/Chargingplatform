#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QNetworkAccessManager;
class QPushButton;
class QTimer;
class QWebEngineView;

namespace ncs {

struct RoutePlan;

// UC-U-04 增强：站内「导航到这里」改为 App 内嵌驾车路线。
// 起点为用户模拟定位，终点为电站；驾车路线经腾讯 WebService 路线规划接口
// 计算后绘制在 QtWebEngine 内嵌腾讯地图上（不再跳转浏览器）。
// 仅 NCS_HAS_WEBENGINE 构建使用。无网 / 接口失败 / 地图加载失败时降级：
// 静态地图照常显示 + 失败原因 +「浏览器打开路线」按钮兜底。
class MapRouteDialog : public QDialog
{
    Q_OBJECT

public:
    // 坐标为纬度、经度（与腾讯 WebService 一致：纬度在前）
    MapRouteDialog(double fromLatitude, double fromLongitude,
                   double toLatitude, double toLongitude,
                   const QString &stationName, const QString &stationAddress,
                   QWidget *parent = nullptr);
    ~MapRouteDialog() override;

private:
    void loadFallback(const QString &message);
    void onDrivingPlan(const RoutePlan &plan);
    void onWalkingPlan(const RoutePlan &plan);
    void onOsrmPlan(const RoutePlan &plan);
    bool acceptRoute(const RoutePlan &plan);
    void startReadyPolling();
    void injectRoute(const QString &script, const QString &summary);
    void failRoute(const QString &message);
    void openBrowserRoute();

    QWebEngineView *view_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QPushButton *browserButton_ = nullptr;
    QNetworkAccessManager *network_ = nullptr;
    QTimer *readyTimer_ = nullptr;
    QString htmlPath_;
    double fromLatitude_ = 0.0;
    double fromLongitude_ = 0.0;
    double toLatitude_ = 0.0;
    double toLongitude_ = 0.0;
    QString stationName_;
    QString stationAddress_;
    bool ready_ = false;          // 页面内腾讯地图初始化完成
    bool routeInjected_ = false;  // 路线已成功注入
    bool walkingTried_ = false;   // 驾车不可用时已尝试腾讯步行
    bool osrmTried_ = false;      // 步行仍失败时已尝试 OSRM 备用路网
    int readyTicks_ = 0;
    bool pendingStraight_ = false;  // 失败降级：暂存直线参考线注入脚本
    QString pendingScript_;       // 地图未就绪时暂存的注入脚本
    QString pendingSummary_;
};

}  // namespace ncs
