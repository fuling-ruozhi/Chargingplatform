#pragma once

#include <QUrl>

namespace ncs {

// 打开外部浏览器/导航链接。优先走 xdg-desktop-portal 的 OpenURI
// （QDesktopServices 在无完整桌面会话时静默失败）；portal 不可用或
// 失败时回退到 QDesktopServices::openUrl。windowId 用于 portal 的
// parent window（x11:0x<hex>），0 表示不关联窗口。
bool openExternalUrl(const QUrl &url, quintptr windowId);

}
