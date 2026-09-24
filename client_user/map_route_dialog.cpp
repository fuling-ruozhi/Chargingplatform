#include "map_route_dialog.h"

#include "map_route_planner.h"
#include "navigation_launcher.h"
#include "util/geo_distance.h"
#include "util/navigation_url.h"
#include "util/route_decoder.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QWidget>

#include <algorithm>

namespace ncs {

namespace {

constexpr int kReadyPollTicks = 100;  // 150ms × 100 ≈ 15s 地图就绪上限

// JS 单引号字符串转义：模板占位符位于 '...' 字面量内。
QString jsString(const QString &text)
{
    QString escaped = text;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('\''), QStringLiteral("\\'"));
    escaped.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    escaped.replace(QLatin1Char('\r'), QStringLiteral("\\r"));
    escaped.replace(QLatin1Char('\t'), QStringLiteral("\\t"));
    return escaped;
}

// HTML 实体转义：InfoWindow 的 content 是 JS 单引号字符串内的拼接 HTML。
// 单引号必须一并转义（&#39; 为纯文本，不破坏 JS 串；浏览器解析 content
// 为 HTML 时还原为 '），否则含单引号的站点名会提前闭合 JS 字符串。
QString htmlEscape(const QString &text)
{
    QString escaped = text;
    escaped.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    escaped.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    escaped.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    escaped.replace(QLatin1Char('\''), QStringLiteral("&#39;"));
    return escaped;
}

}  // namespace

MapRouteDialog::MapRouteDialog(double fromLatitude, double fromLongitude,
                               double toLatitude, double toLongitude,
                               const QString &stationName,
                               const QString &stationAddress, QWidget *parent)
    : QDialog(parent), fromLatitude_(fromLatitude),
      fromLongitude_(fromLongitude), toLatitude_(toLatitude),
      toLongitude_(toLongitude), stationName_(stationName),
      stationAddress_(stationAddress)
{
    setWindowTitle(QStringLiteral("导航路线 · %1").arg(stationName));
    resize(680, 620);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *header = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 8, 12, 8);
    headerLayout->setSpacing(8);
    statusLabel_ = new QLabel(QStringLiteral("正在规划驾车路线…"), header);
    statusLabel_->setStyleSheet(QStringLiteral("color:#1E88E5;font-weight:600;"));
    browserButton_ = new QPushButton(QStringLiteral("浏览器打开路线"), header);
    browserButton_->setObjectName(QStringLiteral("ghostButton"));
    browserButton_->hide();
    connect(browserButton_, &QPushButton::clicked,
            this, &MapRouteDialog::openBrowserRoute);
    headerLayout->addWidget(statusLabel_, 1);
    headerLayout->addWidget(browserButton_);
    layout->addWidget(header);

    const QString mapKey = qEnvironmentVariable("NCS_TENCENT_MAP_KEY");
    if (mapKey.isEmpty()) {
        loadFallback(QStringLiteral("未配置腾讯地图 Key（环境变量 "
                                    "NCS_TENCENT_MAP_KEY）。"));
        return;
    }

    // 模板 → 临时 HTML 文件 → load(file://)。不用 setHtml：
    // data: URL 在 QtWebEngine 中禁用 localStorage，腾讯地图会白屏。
    QFile templateFile(QStringLiteral(":/client_user/map_template.html"));
    QString html;
    if (templateFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        html = QString::fromUtf8(templateFile.readAll());
    }
    if (html.isEmpty()) {
        loadFallback(QStringLiteral("地图模板资源缺失。"));
        return;
    }

    const double straightKm = GeoDistance::haversineKm(
        fromLongitude_, fromLatitude_, toLongitude_, toLatitude_);
    html.replace(QStringLiteral("__KEY__"), mapKey)
        .replace(QStringLiteral("__LNG__"), QString::number(toLongitude_, 'f', 6))
        .replace(QStringLiteral("__LAT__"), QString::number(toLatitude_, 'f', 6))
        .replace(QStringLiteral("__NAME_ESC__"), jsString(stationName_))
        .replace(QStringLiteral("__NAME_HTML__"), htmlEscape(stationName_))
        .replace(QStringLiteral("__ADDR_HTML__"), htmlEscape(stationAddress_))
        .replace(QStringLiteral("__DIST__"),
                 straightKm >= 0.0 ? QString::number(straightKm, 'f', 2)
                                   : QStringLiteral("—"))
        // 导航模式恒有起点（用户模拟定位）
        .replace(QStringLiteral("__USER_HAS__"), QStringLiteral("true"))
        .replace(QStringLiteral("__USER_LNG__"),
                 QString::number(fromLongitude_, 'f', 6))
        .replace(QStringLiteral("__USER_LAT__"),
                 QString::number(fromLatitude_, 'f', 6));

    // 唯一临时文件名（pid + 时间戳），析构时清理；0600 防位置信息泄露。
    htmlPath_ = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/ncs_route_%1_%2.html")
              .arg(QCoreApplication::applicationPid())
              .arg(QDateTime::currentMSecsSinceEpoch());
    QFile htmlFile(htmlPath_);
    bool htmlReady = false;
    if (htmlFile.open(QIODevice::WriteOnly | QIODevice::Truncate
                      | QIODevice::Text)) {
        const QByteArray payload = html.toUtf8();
        htmlReady = htmlFile.write(payload) == payload.size();
        htmlFile.setPermissions(QFileDevice::ReadOwner
                                | QFileDevice::WriteOwner);
        htmlFile.close();
    }
    if (!htmlReady) {
        loadFallback(QStringLiteral("无法生成地图临时文件。"));
        return;
    }

    view_ = new QWebEngineView(this);
    view_->settings()->setAttribute(
        QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    view_->settings()->setAttribute(
        QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    connect(view_, &QWebEngineView::loadFinished, this, [this](bool ok) {
        if (ok) {
            startReadyPolling();
        } else {
            failRoute(QStringLiteral("地图页面加载失败"));
        }
    });
    view_->load(QUrl::fromLocalFile(htmlPath_));
    layout->addWidget(view_, 1);

    network_ = new QNetworkAccessManager(this);
    requestTencentDriving(network_, this, fromLatitude_, fromLongitude_,
                          toLatitude_, toLongitude_, mapKey,
                          [this](const RoutePlan &plan) {
                              onDrivingPlan(plan);
                          });
}

MapRouteDialog::~MapRouteDialog()
{
    if (!htmlPath_.isEmpty()) {
        QFile::remove(htmlPath_);
    }
}

void MapRouteDialog::loadFallback(const QString &message)
{
    statusLabel_->setText(message);
    statusLabel_->setStyleSheet(
        QStringLiteral("color:#E53935;font-weight:600;"));
    browserButton_->show();
    // 无地图时其余区域留白
    static_cast<QVBoxLayout *>(layout())->addWidget(new QWidget(this), 1);
}

void MapRouteDialog::onDrivingPlan(const RoutePlan &plan)
{
    if (acceptRoute(plan)) return;
    walkingTried_ = true;
    statusLabel_->setText(
        QStringLiteral("驾车路线暂不可用，正在尝试步行参考路线…"));
    statusLabel_->setStyleSheet(
        QStringLiteral("color:#1E88E5;font-weight:600;"));
    requestTencentWalking(network_, this, fromLatitude_, fromLongitude_,
                          toLatitude_, toLongitude_,
                          qEnvironmentVariable("NCS_TENCENT_MAP_KEY"),
                          [this](const RoutePlan &plan) {
                              onWalkingPlan(plan);
                          });
}

void MapRouteDialog::onWalkingPlan(const RoutePlan &plan)
{
    if (acceptRoute(plan)) return;
    osrmTried_ = true;
    statusLabel_->setText(
        QStringLiteral("步行路线暂不可用，正在尝试备用路网…"));
    statusLabel_->setStyleSheet(
        QStringLiteral("color:#1E88E5;font-weight:600;"));
    requestOsrmDriving(network_, this, fromLatitude_, fromLongitude_,
                       toLatitude_, toLongitude_,
                       [this](const RoutePlan &plan) { onOsrmPlan(plan); });
}

void MapRouteDialog::onOsrmPlan(const RoutePlan &plan)
{
    if (acceptRoute(plan)) return;
    failRoute(plan.message);
}

bool MapRouteDialog::acceptRoute(const RoutePlan &plan)
{
    if (!plan.ok || plan.points.size() < 2 || routeInjected_) return false;
    QJsonArray coordinates;
    for (const RoutePoint &point : plan.points) {
        coordinates.append(point.lat);
        coordinates.append(point.lng);
    }
    const QString coordinatesJson = QString::fromUtf8(
        QJsonDocument(coordinates).toJson(QJsonDocument::Compact));
    const QString script = QStringLiteral(
        "window.showDrivingRoute && "
        "window.showDrivingRoute(%1, %2, %3);")
                               .arg(coordinatesJson)
                               .arg(plan.distanceMeters, 0, 'f', 0)
                               .arg(plan.durationMinutes, 0, 'f', 1);
    const QString summary =
        QStringLiteral("%1 %2 km · 约 %3 分钟")
            .arg(plan.label)
            .arg(plan.distanceMeters / 1000.0, 0, 'f', 1)
            .arg(std::max(1.0, plan.durationMinutes), 0, 'f', 0);
    injectRoute(script, summary);
    return true;
}

void MapRouteDialog::startReadyPolling()
{
    readyTicks_ = 0;
    // loadFinished 按契约只触发一次；此处防御重复进入（如页面重载），
    // 复用已有定时器，避免重复分配与多路轮询竞态。
    if (!readyTimer_) {
        readyTimer_ = new QTimer(this);
        readyTimer_->setInterval(150);
        connect(readyTimer_, &QTimer::timeout, this, [this] {
            view_->page()->runJavaScript(
                QStringLiteral("window.__ncsMapReady === true"),
                [this](const QVariant &result) {
                    if (result.toBool()) {
                        readyTimer_->stop();
                        ready_ = true;
                        if (pendingStraight_) {
                            // 失败降级的直线参考线在地图就绪后再补画
                            const QString script = pendingScript_;
                            pendingStraight_ = false;
                            pendingScript_.clear();
                            view_->page()->runJavaScript(script);
                        } else if (!pendingScript_.isEmpty()) {
                            const QString script = pendingScript_;
                            const QString summary = pendingSummary_;
                            pendingScript_.clear();
                            pendingSummary_.clear();
                            injectRoute(script, summary);
                        }
                        return;
                    }
                    if (++readyTicks_ >= kReadyPollTicks) {
                        readyTimer_->stop();
                        if (pendingScript_.isEmpty()
                            && !pendingStraight_) {
                            failRoute(QStringLiteral(
                                "腾讯地图加载超时，请检查网络与 Key"));
                        }
                    }
                });
        });
    }
    readyTimer_->start();
}

void MapRouteDialog::injectRoute(const QString &script,
                                 const QString &summary)
{
    if (routeInjected_) return;
    if (!ready_) {
        pendingScript_ = script;
        pendingSummary_ = summary;
        return;
    }
    view_->page()->runJavaScript(script);
    routeInjected_ = true;
    statusLabel_->setText(summary);
    statusLabel_->setStyleSheet(
        QStringLiteral("color:#0B8043;font-weight:600;"));
}

void MapRouteDialog::failRoute(const QString &message)
{
    if (routeInjected_) return;
    pendingScript_.clear();
    pendingSummary_.clear();
    // 三层算路源都不可用（通常为断网）：画直线参考线 + 说明 + 浏览器兜底
    const QString straightScript =
        QStringLiteral("window.showStraightRoute && "
                       "window.showStraightRoute(%1, %2, %3, %4);")
            .arg(fromLatitude_, 0, 'f', 6)
            .arg(fromLongitude_, 0, 'f', 6)
            .arg(toLatitude_, 0, 'f', 6)
            .arg(toLongitude_, 0, 'f', 6);
    if (ready_) {
        view_->page()->runJavaScript(straightScript);
    } else {
        pendingStraight_ = true;
        pendingScript_ = straightScript;
    }
    const QString text = message.isEmpty()
        ? QStringLiteral("无法获取路线（网络或路线服务不可用）。"
                         "已显示直线参考，可点右侧按钮打开网页路线。")
        : QStringLiteral("路线获取失败：%1。已显示直线参考，"
                         "可点右侧按钮打开网页路线。")
              .arg(message);
    statusLabel_->setText(text);
    statusLabel_->setStyleSheet(
        QStringLiteral("color:#E53935;font-weight:600;"));
    browserButton_->show();
}

void MapRouteDialog::openBrowserRoute()
{
    const QUrl url = NavigationUrl::build(
        fromLongitude_, fromLatitude_, toLongitude_, toLatitude_,
        stationName_);
    if (!openExternalUrl(url, winId())) {
        failRoute(QStringLiteral("无法打开系统浏览器，请按页面坐标手动导航"));
    }
}

}  // namespace ncs
