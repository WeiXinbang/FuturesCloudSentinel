# 期货云端预警系统 - 通信协议规范 (V1.0)

## 1. 基础通信层
*   **协议**: TCP
*   **数据包格式**: Length-Prefix (长度前缀)
    ```
    [ Header (4 Bytes) ] + [ Body (N Bytes) ]
    ```
    *   **Header**: ASCII 字符串表示的十进制整数 (例如 `" 120"`), 表示 Body 的长度。
    *   **Body**: UTF-8 编码的 JSON 字符串。

## 2. 业务数据定义 (JSON)

所有消息必须包含 `type` 字段。

### A. 用户账户 (User Account)

#### 1. 注册 (Register)
*   **方向**: Client -> Server
```json
{
    "type": "register",
    "username": "client001",
    "password": "hashed_password_here" 
}
```

#### 2. 登录 (Login)
*   **方向**: Client -> Server
```json
{
    "type": "login",
    "username": "client001",
    "password": "hashed_password_here"
}
```

#### 3. 设置/修改接收邮箱 (Set Email)
*   **方向**: Client -> Server
```json
{
    "type": "set_email",
    "email": "alert@example.com"
}
```

### B. 预警单管理 (Warning Orders)

#### 4. 添加预警单 (Add Warning)

**场景一：价格预警**
```json
{
    "type": "add_warning",
    "warning_type": "price",
    "account": "88001234",       // 客户期货账号
    "symbol": "rb2310",          // 合约代码
    "trigger_price": 3500.0,     // 触发价格
    "condition": "gte"           // "gte" (>=), "lte" (<=)
}
```

**场景二：时间预警**
```json
{
    "type": "add_warning",
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
    "order_id": "1001"
}
```

#### 6. 修改预警单 (Modify Warning)
*   **方向**: Client -> Server
```json
{
    "type": "modify_warning",
    "order_id": "1001",
    "trigger_price": 3600.0,
    "condition": "lte"
}
```

### C. 查询与推送 (Query & Push)

#### 7. 查询预警单 (Query Warnings)
*   **方向**: Client -> Server
```json
{
    "type": "query_warnings",
    "status_filter": "active" // "active", "triggered", "deleted", "all"
}
```

#### 8. 服务器响应 (Server Response)
```json
{
    "type": "response",
    "request_type": "add_warning",
    "success": true,
    "message": "添加成功",
    "data": {
        "order_id": "1002"
    }
}
```

#### 9. 预警触发通知 (Alert Triggered)
*   **方向**: Server -> Client
```json
{
    "type": "alert_triggered",
    "order_id": "1001",
    "symbol": "rb2310",
    "trigger_value": 3501.0,
    "trigger_time": "2023-11-25 15:00:01", // 触发时间
    "message": "您的螺纹钢合约已达到触发价格 3500.0！"
}
```
