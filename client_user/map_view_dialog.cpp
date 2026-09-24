#include "map_view_dialog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineSettings>
#include <QWebEngineView>

namespace ncs {

namespace {

// JS 单引号字符串转义：模板占位符位于 '...' 字面量内，需转义反斜杠/引号/换行。
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

// HTML 实体转义：InfoWindow 的 content 是拼接的 HTML，名称/地址中的
// < > & " 必须转义，否则可被注入为任意 HTML/脚本。
QString htmlEscape(const QString &text)
{
    QString escaped = text;
    escaped.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    escaped.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    escaped.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    return escaped;
}

}  // namespace

MapViewDialog::MapViewDialog(double stationLongitude, double stationLatitude,
                             const QString &stationName, const QString &stationAddress,
                             double distanceKm, QWidget *parent, bool hasUserPosition,
                             double userLongitude, double userLatitude)
    : QDialog(parent), longitude_(stationLongitude), latitude_(stationLatitude),
      name_(stationName), address_(stationAddress), distanceKm_(distanceKm),
      hasUserPosition_(hasUserPosition), userLongitude_(userLongitude),
      userLatitude_(userLatitude)
{
    setWindowTitle(QStringLiteral("电站地图 · %1").arg(stationName));
    resize(680, 580);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *title = new QLabel(
        QStringLiteral("%1 · %2 · 距您 %3 km")
            .arg(stationName, stationAddress)
            .arg(distanceKm_ >= 0.0 ? QString::number(distanceKm_, 'f', 2)
                                    : QStringLiteral("—")),
        this);
    title->setObjectName(QStringLiteral("mutedLabel"));
    title->setContentsMargins(12, 10, 12, 6);
    layout->addWidget(title);

    const QString mapKey = qEnvironmentVariable("NCS_TENCENT_MAP_KEY");
    if (mapKey.isEmpty()) {
        auto *notice = new QLabel(
            QStringLiteral("未配置腾讯地图 Key（环境变量 NCS_TENCENT_MAP_KEY）。\n"
                           "请在启动前设置 Key，或使用“导航到这里”浏览器路线。"),
            this);
        notice->setWordWrap(true);
        notice->setStyleSheet(QStringLiteral("color:#E53935;padding:16px;"));
        layout->addWidget(notice, 1);
        auto *closeButton = new QPushButton(QStringLiteral("关闭"), this);
        closeButton->setObjectName(QStringLiteral("ghostButton"));
        connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
        auto *buttonRow = new QHBoxLayout;
        buttonRow->addStretch();
        buttonRow->addWidget(closeButton);
        layout->addLayout(buttonRow);
        return;
    }

    // 模板 → 临时 HTML 文件 → load(file://)。
    // 不用 setHtml：它会生成 data: URL，QtWebEngine 在其中禁用 localStorage，
    // 而腾讯地图 JS 依赖 localStorage，会抛 SecurityError 导致整页白屏。
    QFile templateFile(QStringLiteral(":/client_user/map_template.html"));
    QString html;
    if (templateFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        html = QString::fromUtf8(templateFile.readAll());
    }
    if (html.isEmpty()) {
        auto *notice = new QLabel(
            QStringLiteral("地图模板资源缺失，请使用“导航到这里”浏览器路线。"), this);
        notice->setWordWrap(true);
        notice->setStyleSheet(QStringLiteral("color:#E53935;padding:16px;"));
        layout->addWidget(notice, 1);
        return;
    }

    html.replace(QStringLiteral("__KEY__"), mapKey)
        .replace(QStringLiteral("__LNG__"), QString::number(longitude_, 'f', 6))
        .replace(QStringLiteral("__LAT__"), QString::number(latitude_, 'f', 6))
        // marker title 里是 JS 字符串 → JS 转义
        .replace(QStringLiteral("__NAME_ESC__"), jsString(name_))
        // InfoWindow content 里是拼接 HTML → HTML 实体转义
        .replace(QStringLiteral("__NAME_HTML__"), htmlEscape(name_))
        .replace(QStringLiteral("__ADDR_HTML__"), htmlEscape(address_))
        .replace(QStringLiteral("__DIST__"),
                 distanceKm_ >= 0.0 ? QString::number(distanceKm_, 'f', 2)
                                    : QStringLiteral("—"))
        .replace(QStringLiteral("__USER_HAS__"),
                 hasUserPosition_ ? QStringLiteral("true") : QStringLiteral("false"))
        .replace(QStringLiteral("__USER_LNG__"),
                 QString::number(userLongitude_, 'f', 6))
        .replace(QStringLiteral("__USER_LAT__"),
                 QString::number(userLatitude_, 'f', 6));

    // 唯一临时文件名（pid + 时间戳），避免多个对话框互相覆盖；析构时清理。
    htmlPath_ = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/ncs_map_%1_%2.html")
              .arg(QCoreApplication::applicationPid())
              .arg(QDateTime::currentMSecsSinceEpoch());
    QFile htmlFile(htmlPath_);
    bool htmlReady = false;
    if (htmlFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        const QByteArray payload = html.toUtf8();
        htmlReady = htmlFile.write(payload) == payload.size();
        // 临时文件含坐标等位置信息：收缩为仅当前用户可读写（0600）。
        htmlFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        htmlFile.close();
    }
    if (!htmlReady) {
        auto *notice = new QLabel(
            QStringLiteral("无法生成地图临时文件，请使用“导航到这里”浏览器路线。"), this);
        notice->setWordWrap(true);
        notice->setStyleSheet(QStringLiteral("color:#E53935;padding:16px;"));
        layout->addWidget(notice, 1);
        auto *closeButton = new QPushButton(QStringLiteral("关闭"), this);
        closeButton->setObjectName(QStringLiteral("ghostButton"));
        connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
        auto *buttonRow = new QHBoxLayout;
        buttonRow->addStretch();
        buttonRow->addWidget(closeButton);
        layout->addLayout(buttonRow);
        return;
    }

    view_ = new QWebEngineView(this);
    view_->settings()->setAttribute(
        QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    view_->settings()->setAttribute(
        QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    view_->load(QUrl::fromLocalFile(htmlPath_));
    layout->addWidget(view_, 1);
}

MapViewDialog::~MapViewDialog()
{
    if (!htmlPath_.isEmpty()) {
        QFile::remove(htmlPath_);
    }
}

}  // namespace ncs
