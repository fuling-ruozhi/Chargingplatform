# NCS Phase 4 统一运行说明

## 唯一开发主线

源代码只在 Windows Git 工作区修改：

```text
<project-root>
```

Ubuntu 下的三个 `NCS_linux_verify*` 目录都是验证快照，不是 Git 工作区，不应直接编辑。

| Ubuntu 目录 | 定位 | 使用规则 |
|---|---|---|
| `/home/bit/ncs/NCS_linux_verify_phase4` | 当前 Phase 4 Demo | 唯一允许用于当前编译、测试和演示的目录 |
| `/home/bit/ncs/NCS_linux_verify_phase3demo` | Phase 3 历史参考 | 只用于比较和回滚参考，不启动 |
| `/home/bit/ncs/NCS_linux_verify` | 早期另一套完整业务快照 | 只用于历史参考，不启动；其协议和数据库模型与当前主线不同 |

三个目录的程序不能混用。尤其不能用一个目录的 `ncs_admin` 搭配另一个目录的 `ncs_user`。

## 同步、编译与测试

### Ubuntu 环境依赖

首次在虚拟机上构建前，安装基础构建依赖。若当前分支的顶层 CMake 已声明 `Qt6 ... Charts`，还必须安装 Qt Charts 开发包：

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build qt6-base-dev qt6-tools-dev libqt6charts6-dev
```

如果提示找不到 `libqt6charts6-dev`，先查询当前软件源里的 Qt Charts 包名：

```bash
apt search qt6 charts
```

在 Windows 主源码目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\linux_verify.ps1 `
  -RemoteHost bit@<ubuntu-host> `
  -RemoteDir /home/bit/ncs/NCS_linux_verify_phase4
```

该命令会把当前源码同步到 Phase 4 验证目录，重新配置和构建，并运行完整 CTest。看到最终 `RESULT: PASS` 后才启动 Demo。

## 演示数据库

Phase 4 服务端默认使用：

```text
/home/bit/.local/share/NCS/charge_platform.db
```

该路径由 Qt `QStandardPaths::GenericDataLocation` 决定，与启动时所在目录无关。因此旧版和新版服务端可能访问同一文件，这也是禁止启动旧目录程序的原因之一。

### 重置演示状态

先关闭所有 `ncs_user` 和 `ncs_admin`，然后在 Windows 主源码目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\phase4_demo_reset.ps1 `
  -RemoteHost bit@<ubuntu-host>
```

重置工具会先创建带时间戳的数据库备份，然后：

- 清空 `charging_record` 演示记录。
- 将 `NCS-01-01` 至 `NCS-01-07` 设为 `Idle`。
- 将 `NCS-01-08` 设为 `Fault`。
- 校验重置结果，不符合预期时返回失败。

服务端仍在运行、数据库不正确或八个目标电桩不完整时，工具会拒绝修改。

## 启动顺序

在 Ubuntu 桌面打开第一个终端，只运行 Phase 4 服务端：

```bash
/home/bit/ncs/NCS_linux_verify_phase4/build-linux/client_admin/ncs_admin
```

看到以下内容后保持终端运行：

```text
Server started
127.0.0.1:9527
```

再打开第二个终端，只运行 Phase 4 用户端：

```bash
/home/bit/ncs/NCS_linux_verify_phase4/build-linux/client_user/ncs_user
```

不要从 `NCS_linux_verify` 或 `NCS_linux_verify_phase3demo` 启动同名程序。

## 人工演示流程

1. 在登录页注册演示账号，或使用已有账号登录。
2. 确认进入“附近充电站”页面。
3. 打开“东软软件园充电站”，查看八个充电桩。
4. 选择 `NCS-01-01` 至 `NCS-01-07` 中任意空闲桩，点击“开始充电”。
5. 确认页面从“请求中”进入“充电中”，计时和电量持续变化。
6. 点击“结束充电”。
7. 确认显示“充电已结束，记录已保存”，并显示充电时长、电量和费用。
8. 再次查看该电站，确认目标桩已恢复为空闲，其他充电桩状态未受影响。

## 故障检查

- GUI 与本说明不一致：先用 `readlink -f /proc/<PID>/exe` 确认进程来自 `NCS_linux_verify_phase4`。
- 大量电桩显示“使用中”：停止程序并运行演示重置命令。
- 数据库版本错误：不要启动旧版服务端；重新执行 Phase 4 同步构建命令。
- 客户端连接失败：确认 Phase 4 `ncs_admin` 已监听 `127.0.0.1:9527`。
