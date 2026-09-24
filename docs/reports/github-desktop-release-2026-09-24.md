# Charging Platform GitHub Release Report

## 范围

整理 Ubuntu/Linux Qt 桌面版充电桩管理平台，准备发布至 `fuling-ruozhi/NCS-Charging-Station-Platform`。发布快照仅选取 Qt/C++ 客户端、核心服务、SQLite schema、CMake、桌面项目文档、截图和测试；Vue `web/`、ML `ml/`、其余 Web 脚本以及构建/本地运行产物不纳入快照。仓库内用户端用于嵌入地图的 Qt HTML 资源保留为桌面客户端资源。

## 技术栈

C++17、Qt 6.2+、CMake、Qt SQL/SQLite、Qt Network TCP、Ubuntu/Linux。

## 整理内容

- 重写 `README.md`，记录功能、架构、依赖、构建、启动顺序、项目结构和桌面截图位置。
- 更新 `.gitignore`，覆盖构建目录、Qt/IDE 临时配置、日志、数据库、缓存及仓库内 Vue/ML 目录。
- 发布文件范围：`CMakeLists.txt`、`CMakePresets.json`、`client_admin/`、`client_common/`、`client_user/`、`core/`、`db/`、桌面所需 `docs/` 文件、`tests/`、`LICENSE`、`README.md` 和 `.gitignore`。
- 任务开始时，工作区已有未提交的 C++/测试改动；这些当前项目文件包含在发布快照中，未改写原分支历史。

## 验证

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`：通过。
- `cmake --build build -j$(nproc)`：通过，构建管理端、用户端及 65 个测试目标。
- `ctest --test-dir build --output-on-failure`：65/65 通过。
- 以 `/tmp/ncs_release_smoke_escalated` 为独立数据根目录，短时启动 `ncs_admin` 和 `ncs_user`：SQLite 初始化成功；服务端监听 `127.0.0.1:9527`；管理端和用户端均建立 TCP 连接；`PRAGMA integrity_check` 返回 `ok`，共初始化 18 张表。
- GUI 烟测使用 `QT_QPA_PLATFORM=offscreen`，只验证程序启动、服务监听和连接；未人工操作登录表单及后续业务页面。

## GitHub 状态

- 目标地址：<https://github.com/fuling-ruozhi/NCS-Charging-Station-Platform>
- 匿名 GitHub API 查询返回 HTTP 404；私有仓库也可能返回相同结果，因此无法只凭此结果判断仓库是否存在。
- HTTPS Git 查询需要用户名凭据；SSH 查询因本机 GitHub host key/认证不可用而失败；本机没有 `gh` CLI 或 `GH_TOKEN`/`GITHUB_TOKEN`。
- 因缺少有效 GitHub 认证，未创建/覆盖远端、未推送。现有 `origin`（Gitee）保持不变。
- 本地发布提交：待创建。

## 未解决问题

需要可用的 GitHub 认证以检查目标仓库的私有/已存在状态，必要时创建仓库并推送 `main`。在认证可用前，本报告不宣称已经发布。
