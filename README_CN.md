# NCS 充电站管理平台

## 项目简介

NCS 是一套运行于 Linux 的 Qt 桌面充电站管理平台，采用 C++17 开发。系统由用户端、管理端和 TCP 服务组成，通过 JSON 协议通信，并由服务端使用 SQLite 持久化业务数据。

本仓库面向 Qt 桌面版本，包含 C++/Qt 客户端、服务端核心、数据库 schema、测试和项目文档。

## 系统架构

```text
用户端
  |
TCP JSON 协议
  |
管理端服务
  |
SQLite 数据库
```

管理端启动 TCP 服务（默认监听 `127.0.0.1:9527`），并提供管理界面。用户端通过 TCP 访问业务服务，不直接访问 SQLite。

## 核心功能

- 用户与管理员认证、用户管理
- 充电站与充电桩管理
- 充电状态监控、预约、订单结算与历史记录
- 用户评分与电站评价
- 智能电站推荐
- 管理端数据面板与分析
- 操作日志审计与安全事件记录

## 技术栈

- C++17
- Qt 6（最低 6.2）
- CMake
- SQLite（Qt SQL）
- TCP Socket（Qt Network）
- Ubuntu / Linux 桌面

## 项目结构

```text
client_admin/   管理端界面与 TCP 服务启动入口
client_common/  客户端共用组件
client_user/    用户端界面及其 Qt 资源
core/           业务模型、Repository、Service、数据库和 TCP 协议
db/             SQLite schema 与 Qt 资源
docs/           项目设计、协议和报告
tests/          CMake / CTest 单元与集成测试
```

## 编译与运行

在 Ubuntu 安装 CMake、C++ 构建工具及项目使用的 Qt 开发模块：

```bash
sudo apt update
sudo apt install -y build-essential cmake qt6-base-dev qt6-tools-dev libqt6charts6-dev qt6-multimedia-dev
```

在仓库根目录配置并编译：

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

先启动管理端（它同时启动 TCP 服务并初始化本地 SQLite 数据库）：

```bash
./build/client_admin/ncs_admin
```

再在另一个终端启动用户端：

```bash
./build/client_user/ncs_user
```

## 测试结果

项目测试由 CTest 管理，可在仓库根目录执行：

```bash
ctest --test-dir build --output-on-failure
```

Ubuntu 发布验证中 CTest 的 65 项测试全部通过，覆盖数据库迁移、SQLite 服务、TCP 客户端/服务端集成以及管理端和用户端 UI。完整记录见 `docs/reports/` 中的 GitHub Release 报告。

## 截图

桌面界面截图见 [`docs/reports/phase4-final-demo-screenshots/`](docs/reports/phase4-final-demo-screenshots/) 和 [`docs/reports/screenshots/ui-ecommerce-marketplace-redesign/`](docs/reports/screenshots/ui-ecommerce-marketplace-redesign/)。

## 作者

[fuling-ruozhi](https://github.com/fuling-ruozhi)
