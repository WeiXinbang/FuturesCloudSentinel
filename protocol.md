# 期货云端预警系统 - 通信协议规范 V3

## 1. 基础通信层
*   **协议**: TCP
*   **数据包格式**: Length-Prefix (长度前缀)
    ```
    [ Header (4 Bytes) ] + [ Body (N Bytes) ]
    ```
    *   **Header**: ASCII 字符串表示的十进制整数 (例如 `" 120"`), 表示 Body 的长度。
    *   **Body**: UTF-8 编码的 JSON 字符串。
*   **交互机制**: 
    *   所有客户端发出的请求（Request），服务器都必须回复（Response）。
    *   建议在请求中携带 `request_id`，服务器在响应中回传该 ID，以便异步匹配。

## 2. 业务数据定义 (JSON)

所有消息必须包含 `type` 字段。

### A. 用户账户 (User Account)

#### 1. 注册 (Register)
*   **方向**: Client -> Server
```json
{
    "type": "register",
    "request_id": "req_001",     // 请求ID
    "username": "client001",
    "password": "hashed_password_here" 
}
```

#### 2. 登录 (Login)
*   **方向**: Client -> Server
```json
{
    "type": "login",
    "request_id": "req_002",
    "username": "client001",
    "password": "hashed_password_here"
}
```

#### 3. 设置/修改接收邮箱 (Set Email)
*   **方向**: Client -> Server
```json
{
    "type": "set_email",
    "request_id": "req_003",
    "email": "alert@example.com"
}
```

### B. 预警单管理 (Warning Orders)

#### 4. 添加预警单 (Add Warning)

**场景一：价格预警 (双向阈值)**
*   **说明**: 价格预警现在包含上限和下限。
```json
{
    "type": "add_warning",
    "request_id": "req_004",
    "warning_type": "price",
    "account": "88001234",       // 客户期货账号
    "symbol": "rb2310",          // 合约代码
    "max_price": 3600.0,         // 最大触发价格 (上限)
    "min_price": 3400.0          // 最小触发价格 (下限)
}
```

**场景二：时间预警**
```json
{
    "type": "add_warning",
    "request_id": "req_005",
    "warning_type": "time",
    "account": "88001234",
    "symbol": "rb2310",
    "trigger_time": "2023-11-25 14:55:00"
}
```

#### 5. 删除预警单 (Delete Warning)
*   **方向**: Client -> Server
```json
{
    "type": "delete_warning",
    "request_id": "req_006",
    "order_id": "1001"           // 必须提供预警单ID
}
```

#### 6. 修改预警单 (Modify Warning)
*   **方向**: Client -> Server

**场景一：修改价格预警**
```json
{
    "type": "modify_warning",
    "request_id": "req_007",
    "order_id": "1001",
    "warning_type": "price",     // 明确修改的是价格单
    "max_price": 3650.0,         // 新的上限
    "min_price": 3450.0          // 新的下限
}
```

**场景二：修改时间预警**
```json
{
    "type": "modify_warning",
    "request_id": "req_008",
    "order_id": "1002",
    "warning_type": "time",      // 明确修改的是时间单
    "trigger_time": "2023-11-25 16:00:00" // 新的触发时间
}
```

### C. 查询与推送 (Query & Push)

#### 7. 查询预警单 (Query Warnings)
*   **方向**: Client -> Server
```json
{
    "type": "query_warnings",
    "request_id": "req_008",
    "status_filter": "active" // "active", "triggered", "deleted", "all"
}
```

#### 8. 服务器响应 (Server Response)
*   **说明**: 针对上述所有请求的通用回复结构。
```json
{
    "type": "response",
    "request_id": "req_004",     // 回传对应的请求ID
    "request_type": "add_warning",
    "success": true,
    "message": "添加成功",
    "data": {
        "order_id": "1002"       // 重要：添加成功后，服务器必须返回生成的 ID
    }
}
```

#### 9. 预警触发通知 (Alert Triggered)
*   **方向**: Server -> Client
*   **说明**: 服务器主动推送，客户端收到后建议回复 ACK。
```json
{
    "type": "alert_triggered",
    "alert_id": "msg_9999",      // 通知的唯一ID
    "order_id": "1001",          // 关联的预警单ID
    "symbol": "rb2310",
    "trigger_value": 3601.0,     // 触发时的实际数值
    "trigger_time": "2023-11-25 15:00:01",
    "message": "您的螺纹钢合约已突破上限 3600.0，当前价格 3601.0！"
}
```

#### 10. 预警通知确认 (Alert ACK)
*   **方向**: Client -> Server
*   **说明**: 客户端收到预警后回复，确认已接收。
```json
{
    "type": "alert_ack",
    "alert_id": "msg_9999"
}
```
