# NCS TCP/JSON 协议

版本：1.1
状态：Stage 11 用户、订单、站点及管理端基础认证接口已实现

## 1. Transport

- TCP，默认仅监听本机回环地址。
- 每帧：4 字节大端 payload 长度 + UTF-8 JSON。
- payload 最大 1 MiB；超限关闭连接。
- request/response 通过 `request_id` 关联。

## 2. Envelope

Request：

```json
{"request_id":"uuid","type":"route.name","data":{}}
```

Response：

```json
{"request_id":"uuid","success":true,"code":0,"message":"","data":{}}
```

`protocol_version` 当前为 1。客户端连接后先调用 `system.ping`；不支持的主版本必须拒绝业务请求并提示升级。

## 3. Authentication

`session_token` 是 **Project Design Decision**，用于满足项目说明书的数据安全要求。

- Public：不需要 token。
- User：`data.session_token` 必填，服务端解析 user id。
- Admin：`data.admin_session_token` 必填，服务端解析 admin id。
- 用户和管理员 token 不可互换。
- 禁止在请求中用 `user_id/admin_id` 覆盖 token 身份。

## 4. Public routes

### `system.ping`

Request data：`{}`。Response data：

```json
{"pong":true,"server_time":"本地时间字符串","protocol_version":1}
```

### `user.otp.request`

Request：

```json
{"phone":"13800138000"}
```

Response：

```json
{"cooldown_seconds":60,"expires_seconds":300}
```

课程本地演示可在请求中传 `demo_otp_echo=true`，或启动服务前设置
`NCS_DEMO_OTP_ECHO=1`，此时响应会额外返回 `display_code`。默认不回显验证码，
日志也不得记录验证码。

### `user.login`

Request：

```json
{"phone":"13800138000","code":"123456"}
```

Response：

```json
{
  "session_token":"opaque-token",
  "user":{"id":1,"phone_masked":"138****8000","nickname":"用户8000","avatar_path":"","balance":0.00,"status":1,"created_at":"2026-09-03 10:00:00.000"}
}
```

手机号不存在时自动注册。冻结用户不签发 token。

### `admin.login`

Request：`{"username":"admin","password":"..."}`。Response：管理员基本信息与 `admin_session_token`。账号不存在和密码错误统一返回 `AdminInvalidCredentials`；同一账号连续失败 5 次返回 `AdminLocked` 和 `retry_after_seconds`，服务端在 30 秒锁定期内拒绝正确密码。默认 seed 为 SRS 指定的课程初始账号，数据库只保存 SHA-256 加盐哈希。

## 5. User routes

下列 request 均省略必填的 `session_token`。

### `user.profile.get`

Request：`{}`。Response：当前用户资料；手机号只返回脱敏展示值。

### `user.profile.nickname.update`

Request：`{"nickname":"新昵称"}`。

### `user.profile.avatar.update`

Request：`{"avatar_path":"avatars/1.png"}`。文件选择/复制发生在本地客户端，服务端只保存经校验的相对路径；不得包含盘符或 `..`。

### `user.recharge`

Request：`{"amount":100.00}`。Response：充值金额、充值前后余额、流水 id。

### `user.logout`

使当前 token 立即失效。

## 6. Station routes

### `station.list`

Request：`{"longitude":116.30,"latitude":39.90}`。

Response：

```json
{
  "stations":[{
    "id":1,"name":"示例站","address":"地址","longitude":116.31,"latitude":39.91,
    "price":1.20,"idle_slots":6,"total_slots":8,"distance_km":1.4
  }]
}
```

服务端返回按距离升序的结果；无位置时客户端使用区域预置坐标。

### `station.detail`

Request：`{"station_id":1,"origin_longitude":116.30,"origin_latitude":39.90}`。

Response 含站点字段、distance_km 与 chargers：

```json
{
  "chargers":[{
    "id":1,"station_id":1,"code":"NCS-01-01","type":1,"power_kw":60.0,
    "status":1,"total_count":12,"active_order_status":0
  }]
}
```

`active_order_status`：-1无活动订单、0预约、1充电中；不得返回其他用户 id/order id。

`rating_summary` 为站点评价聚合；无评价时各数值为 `0`：

```json
{"review_count":20,"average_score":4.5,"environment_score":4.7,
 "queue_score":4.1,"equipment_score":4.6,"parking_score":4.4}
```

### `review.submit`

Request：`{"order_id":123,"environment_score":5,"queue_score":4,
"equipment_score":5,"parking_score":3}`。服务端从 session token 和订单读取用户、站点、电桩，
只允许已结算订单评价，并由服务端计算 `overall_score`。

Response：`{"review_id":1,"order_id":123,"overall_score":4.25}`（同时返回评价 DTO）。
客户端不得发送或覆盖 `user_id`、`station_id`、`charger_id`、`overall_score`。

### `review.get`

Request：`{"order_id":123}`。服务端只允许当前用户查询自己的订单，Response 至少包含
`has_review`；已有评价时附带评价 DTO。

## 7. Order and charging routes

### `charge.active`

Request：`{}`。服务端从 token 解析用户。Response：`has_active` 及当前用户的完整 order DTO。

### `charge.reserve`

Request：`{"charger_id":1}`。Response：Reserved order DTO。服务端校验用户正常、余额、唯一活动订单、桩状态，并在事务中占桩建单。

余额低于 `charge.min_balance`（默认5.00元）返回 `InsufficientBalance`；冻结账号返回 `UserFrozen`。预约有效期由服务端配置 `charge.reservation_minutes`（默认15分钟）确定。

### `charge.start`

正式 Request：`{"order_id":123}`。只能把当前用户未过期 Reserved 转 Charging。客户端不再发送 user_id/charger_id 作为身份或主定位。

开始时再次校验账号未冻结及余额达到起充金额，防止预约后状态或余额发生变化。

旧的“无预约直接开始”仅保留为 deprecated legacy compatibility；它仍要求 token，且只接受 `charger_id`，服务端不信任客户端 `user_id`。正式 UI 不调用，旧测试完成迁移后关闭。

### `charge.cancel`

Request：`{"order_id":123}`。只能取消当前用户 Reserved；幂等返回最终 Cancelled DTO。

### `charge.settle`

Request：`{"order_id":123}`。只能结束当前用户 Charging。Completed 重试返回同一小票，不重复扣款/统计。

Response receipt：

```json
{
  "order_no":"NCS...","station_name":"站点","charger_code":"NCS-01-01",
  "start_time":"...","end_time":"...","duration_seconds":3600,
  "energy":60.0,"price_per_kwh":1.20,"amount":72.00,"debt_amount":0.00,
  "balance_after":28.00,"power_kw":60.0,"initial_soc":20.0,"final_soc":100.0,
  "status":2
}
```

### `order.list`

Request：`{"page":1,"page_size":20}`。只返回当前用户订单，`created_at DESC, id DESC`。

### `order.detail`

Request：`{"order_id":123}`。非 owner 返回授权错误，不泄露是否存在。

## 8. Admin routes

所有 route 要求 `admin_session_token`：

- `admin.logout`：立即注销当前管理员 token。
- `admin.summary`：返回 `database_path`、`online_chargers`、`total_chargers`，供管理端状态栏展示；在线口径为非 Fault 的 Idle/Using 电桩。
- `admin.revenue`：`range_days` 只允许7/30；只统计 charging_order status=2。
- `admin.charger.list/create/delete/status/restart`
- `admin.station.list/create/update/delete`
- `admin.station.batchCreateChargers`: requires `station_id`, `prefix`, and `count` (1-100); optional `type` and `power_kw`. The server creates generated `<prefix>-NNN` codes in one transaction.
- `admin.user.list`：可选 `keyword`，按用户 ID、username 或 phone 搜索；返回真实用户字段。
- `admin.user.freeze/unfreeze`：要求 `user_id`，切换 `user.status` 1（正常）与 0（冻结）；冻结用户下一次用户登录返回 `UserFrozen`。
- `admin.user.orders`：要求 `user_id`，只返回该用户的 `charging_order` 记录。
- `admin.user.list/status/orders`
- `admin.prediction.list`：返回 `generated_at`、`items`、`actual`，以及预测脚本运行状态 `running`（true 表示脚本正在后台执行）与 `last_error`（上次失败原因，空表示无错误）。客户端在触发运行后轮询本接口获取进度与结果。
- `admin.prediction.run`：异步触发预测脚本（QProcess），立即返回；脚本已在运行时幂等成功。结果不随本接口返回——通过轮询 `admin.prediction.list` 的 `running` / `last_error` 获取。设计约束：服务器为单线程请求模型，脚本执行不得阻塞网络线程。

每个写 route 由 Service 校验并使用事务；重启/故障/恢复写 `ops_log`；BR-10 在删除站点前显式校验并由 FK RESTRICT 双重保护。

## 9. Error codes

| 范围 | 示例 | 含义 |
|---|---|---|
| 1000-1099 | PROTOCOL_INVALID_DATA | framing/JSON/version/字段 |
| 2000-2099 | NETWORK_UNAVAILABLE | 连接/超时 |
| 3000-3099 | AUTH_REQUIRED/EXPIRED/FORBIDDEN | 会话认证授权 |
| 4000-4099 | USER_INVALID_PHONE/CODE/FROZEN | 用户业务 |
| 4100-4199 | BALANCE_INSUFFICIENT | 余额充值 |
| 5000-5099 | ORDER_ACTIVE/STATE/OWNER/EXPIRED | 订单 |
| 5100-5199 | CHARGER_NOT_IDLE/FAULT/CONFLICT | 电桩 |
| 5200-5299 | SETTLEMENT_FAILED | 计费结算 |
| 6000-6099 | DATABASE_ERROR/MIGRATION_REQUIRED | 数据库 |

具体数值在实现时集中定义，测试锁定；客户端按 code 映射中文，message 作为兼容兜底。

## 10. Compatibility and deprecation

- 原 framing/envelope 不变，字段以向后兼容方式增加。
- `legacy.user.register(username,password)`、`legacy.user.login(username,password)`、无 order 的直接 `charge.start` 标记 deprecated；不再注册无鉴权的 `charge.stop(record_id)`。
- 正式客户端不得调用 deprecated route。
- `charging_record` 不出现在正式 API；只由 v5→v6 migration 读取。
- 账号密码兼容接口实际命名为 `legacy.user.register` 与 `legacy.user.login`；正式 UI 不调用，响应不签发正式 session token。
- protocol 变更、deprecated route 关闭需组长 review，并更新本文件和集成测试。
