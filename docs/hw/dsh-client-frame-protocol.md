# dsh_client 帧协议文档

> **版本**：v1.0（2026-09-06）
> **作者**：CCB（科研助理）
> **目标读者**：CCA T8（P4C5 dsh_client 实现）+ DSH（协议对接）
> **参考来源**：OMT `dsh_client.cpp` (1141 行) + `event_bus.h` (80 行)
> **协议设计**：100% 兼容 OMT PC Adapter 统一终端协议

---

## 1. 概述

dsh_client 是 P4C5 终端的 WebSocket 客户端，连接到 DSH Agent 适配层（Mac/PC 端）。协议采用 **JSON-over-WebSocket** 设计，与 OMT Tab5 项目 100% 兼容。

**核心特征**：
- **传输层**：WebSocket（RFC 6455），支持 ws:// 和 wss://（TLS）
- **数据格式**：JSON 帧（文本类型 0x01）+ 二进制帧（音频 Opus）
- **帧分类**：14 类下行事件 + 4 类上行帧
- **心跳机制**：WebSocket 内置 ping/pong，30s 间隔
- **重连策略**：指数退避，最大 30 次 × 10s = ~5 分钟
- **多包拼接**：大 JSON（>4KB）自动分包接收并重组

**与 OMT 差异**：
- OMT 面向 Tab5（WiFi only），P4C5 新增 4G 支持（WiFi↔4G 自动切换）
- P4C5 新增 `status_report` 上行帧（电池/信号/温度）
- 其余帧格式、事件类型、状态机 100% 一致

---

## 2. WebSocket 连接

### 2.1 连接 URL 格式

```
ws://<host>:<port>/<path>
wss://<host>:<port>/<path>     (TLS)
```

示例：
```
ws://192.168.1.10:8080/agent
ws://dsh.example.com:443/agent
wss://dsh.example.com:443/agent
```

> **注意**：与 OMT 一致，URL 中不含 device_id/token 查询参数（认证通过应用层 JSON 帧完成）。

### 2.2 连接参数

| 参数 | 值 | 来源 |
|---|---|---|
| buffer_size | 4096 bytes | OMT 实测稳定值（16KB 会导致栈溢出）|
| task_stack | 8192 bytes | WS 事件处理任务栈 |
| ping_interval | 30s | WebSocket 内置 ping |
| pong_timeout | 60s | 60s 未收到 pong 视为超时 |
| network_timeout | 10000ms | 连接建立超时 |
| reconnect_delay | 10000ms | 重连间隔 |
| max_reconnect | 30 次 | 超过后放弃，进入 ERROR 状态 |

### 2.3 连接流程图

```
  P4C5 终端                       DSH Adapter (Mac/PC)
     │                                  │
     │── WS Upgrade Request ──────────→│
     │←─ WS Upgrade Response (101) ────│
     │                                  │
     │   ┌──────────────────────┐       │
     │   │ 状态: CONNECTED      │       │
     │   └──────────────────────┘       │
     │                                  │
     │◄══ WebSocket ping (30s) ════════▶│
     │◄══ WebSocket pong ══════════════▶│
     │                                  │
     │── {type:"text"} ───────────────→│  用户文本
     │←─ {type:"session_state"} ───────│  会话状态
     │←─ {type:"thinking"} ────────────│  Agent 思考
     │←─ {type:"assistant_text"} ──────│  回复文本（流式）
     │←─ {type:"tool_call"} ───────────│  工具调用
     │←─ {type:"tool_result"} ─────────│  工具结果
     │←─ {type:"assistant_done"} ──────│  回复结束
     │                                  │
     │── {type:"session_ctrl"} ───────→│  会话控制
     │   action: "new"|"cancel"|"stop"  │
     │                                  │
     │══ Opus 二进制帧 ═══════════════▶│  音频上行
     │◄══ Opus 二进制帧 ═══════════════│  音频下行
     │                                  │
     │←─ WS Close Frame ───────────────│  服务端关闭
     │── WS Close Frame ──────────────→│  客户端确认
     │                                  │
     │   ┌──────────────────────┐       │
     │   │ 状态: RECONNECTING   │       │
     │   │ (10s 后重试, ≤30次)  │       │
     │   └──────────────────────┘       │
```

### 2.4 状态机

```
                    ┌────────────┐
                    │DISCONNECTED│←──────────────────┐
                    └─────┬──────┘                   │
                          │ connect()                │ reconnect timeout
                          ▼                          │ (>30 次)
                    ┌────────────┐                   │
                    │ CONNECTING │───────────────────┤
                    └─────┬──────┘    连接失败       │
                          │ WS 101                   │
                          ▼                          │
                    ┌────────────┐                   │
              ┌────→│ CONNECTED  │───────────────────┤
              │     └─────┬──────┘    连接断开       │
              │           │ disconnect()             │
              │           ▼                          │
              │     ┌──────────────┐                 │
              │     │RECONNECTING  │─────────────────┘
              │     └──────┬───────┘   10s 后重连
              │            │ 重连成功
              └────────────┘
              
              任意状态 ──ERROR──→ ERROR (需手动恢复)
```

### 2.5 错误处理

| 场景 | 行为 | 恢复 |
|---|---|---|
| 连接失败 | 10s 后重连（最多 30 次）| 自动 |
| pong 超时（60s 无响应）| 断开 + 重连 | 自动 |
| 服务端关闭 | 断开 + 重连 | 自动 |
| 30 次重连均失败 | 进入 ERROR 状态 | 手动（`dsh_client_start_conversation()`）|
| JSON 解析失败 | 丢弃该帧，继续监听 | 自动 |
| 内存不足 | 日志告警，不崩溃 | 自动 |

---

## 3. 下行帧规范（14 类事件）

> 所有下行帧均为 JSON 文本格式（WebSocket opcode 0x01）。
> 每个帧必须包含 `type` 字段，其余字段按类型不同。

### 3.1 session_state — 会话状态变更

| 字段 | 类型 | 说明 |
|---|---|---|
| type | string | `"session_state"` |
| state | string | `idle` / `running` / `requires_action` / `completed` / `error` |
| model | string | 模型名称（可选）|
| agent_name | string | Agent 名称（可选）|
| session_id | string | 会话 ID（可选）|

```json
{"type":"session_state", "state":"running", "model":"claude-sonnet-4-20250514", "agent_name":"claude"}
```

### 3.2 thinking — Agent 思考内容

| 字段 | 类型 | 说明 |
|---|---|---|
| type | string | `"thinking"` |
| content | string | 思考内容增量文本 |
| tokens_estimate | number | 预估 token 数（可选）|

```json
{"type":"thinking", "content":"让我分析一下这个问题...", "tokens_estimate":150}
```

### 3.3 assistant_text — 回复文本（流式增量）

| 字段 | 类型 | 说明 |
|---|---|---|
| type | string | `"assistant_text"` |
| content | string | 回复文本增量 |
| session_id | string | 会话 ID（可选）|

```json
{"type":"assistant_text", "content":"Hello! I can help you with that."}
```

> ⚠️ **大帧注意**：assistant_text 可能超过 4096 字节（实测 1326+1330+695+498 = 3849 字节分包）。P4C5 需实现多包拼接（参考 OMT `s_ws_rx_buffer` 机制）。

### 3.4 assistant_done — 回复结束

| 字段 | 类型 | 说明 |
|---|---|---|
| type | string | `"assistant_done"` |
| stop_reason | string | `end_turn` / `max_tokens` / `tool_use` |
| usage | object | `{input_tokens, output_tokens}` |
| duration_ms | number | 处理耗时（毫秒）|
| context_usage | number | 上下文窗口占用比例（-1 = 不提供）|

```json
{"type":"assistant_done", "stop_reason":"end_turn", "usage":{"input_tokens":1200, "output_tokens":350}, "duration_ms":4500, "context_usage":0.15}
```

### 3.5 tool_call — 工具调用请求

| 字段 | 类型 | 说明 |
|---|---|---|
| type | string | `"tool_call"` |
| call_id | string | 调用 ID（用于关联 tool_result）|
| tool_name | string | 工具名称 |
| arguments | string | 参数 JSON 字符串 |
| display | object | UI 展示信息 `{title: "..."}` |

```json
{"type":"tool_call", "call_id":"toolu_01ABC", "tool_name":"Read", "arguments":"{\"file_path\":\"/src/main.c\"}", "display":{"title":"Reading main.c"}}
```

### 3.6 tool_result — 工具执行结果

| 字段 | 类型 | 说明 |
|---|---|---|
| type | string | `"tool_result"` |
| call_id | string | 关联的 tool_call ID |
| is_error | boolean | 是否错误 |
| output | string | 输出内容 |
| file_changes | array | 文件变更列表（可选）|

```json
{"type":"tool_result", "call_id":"toolu_01ABC", "is_error":false, "output":"file contents here..."}
```

### 3.7 tool_progress — 工具执行进度

| 字段 | 类型 | 说明 |
|---|---|---|
| type | string | `"tool_progress"` |
| call_id | string | 关联的 tool_call ID |
| elapsed_seconds | number | 已执行秒数 |
| heartbeat | boolean | 心跳标记（保活）|

```json
{"type":"tool_progress", "call_id":"toolu_01ABC", "elapsed_seconds":15.5, "heartbeat":true}
```

### 3.8 file_change — 文件变更

| 字段 | 类型 | 说明 |
|---|---|---|
| type | string | `"file_change"` |
| action | string | `create` / `modify` / `delete` |
| path | string | 文件路径 |

```json
{"type":"file_change", "action":"modify", "path":"/src/main.c"}
```

### 3.9 authorization/requested — 授权请求

| 字段 | 类型 | 说明 |
|---|---|---|
| type | string | `"authorization/requested"` |
| request_id | string | 请求 ID（用于 auth_response）|
| tool_name | string | 需要授权的工具 |
| title | string | 授权标题 |
| description | string | 授权描述 |
| risk_level | string | `low` / `medium` / `high` |

```json
{"type":"authorization/requested", "request_id":"req_01XYZ", "tool_name":"Bash", "title":"Run git push", "description":"Push to origin/main", "risk_level":"medium"}
```

### 3.10 authorization/resolved — 授权已处理

| 字段 | 类型 | 说明 |
|---|---|---|
| type | string | `"authorization/resolved"` |
| request_id | string | 已处理的请求 ID |
| decision | string | `allow` / `deny` |

```json
{"type":"authorization/resolved", "request_id":"req_01XYZ", "decision":"allow"}
```

### 3.11 question/requested — Agent 向用户提问

| 字段 | 类型 | 说明 |
|---|---|---|
| type | string | `"question/requested"` |
| request_id | string | 请求 ID |
| content | string | 问题文本 |

```json
{"type":"question/requested", "request_id":"q_01DEF", "content":"Which file should I modify?"}
```

### 3.12 question/resolved — 问答已完成

| 字段 | 类型 | 说明 |
|---|---|---|
| type | string | `"question/resolved"` |
| request_id | string | 已完成的请求 ID |

```json
{"type":"question/resolved", "request_id":"q_01DEF"}
```

### 3.13 voice_input / voice_output — 音频事件

| 字段 | 类型 | 说明 |
|---|---|---|
| type | string | `"voice_input"` 或 `"voice_output"` |
| content | string | 状态描述（可选）|

```json
{"type":"voice_input", "content":"start"}
{"type":"voice_output", "content":"playing"}
```

> 音频数据通过 **WebSocket 二进制帧**（opcode 0x02）传输，Opus 编码。

### 3.14 系统帧（error / pong / asr_result / aec）

| type 值 | 说明 | 关键字段 |
|---|---|---|
| `"error"` | 错误报告 | `content`: 错误信息 |
| `"pong"` | 心跳回复 | — |
| `"asr_result"` | ASR 识别结果 | `content`: 识别文本（填充输入框，不自动发送）|
| `"aec_enabled"` | AEC 已开启 | — |
| `"aec_disabled"` | AEC 已关闭 | — |

```json
{"type":"error", "content":"Session timeout"}
{"type":"pong"}
{"type":"asr_result", "content":"今天天气怎么样"}
```

---

## 4. 上行帧规范（4 类）

### 4.1 text — 用户文本

```json
{"type":"text", "content":"帮我写一个函数"}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| type | string | `"text"` |
| content | string | 用户输入文本 |

### 4.2 session_ctrl — 会话控制

```json
{"type":"session_ctrl", "action":"new"}
{"type":"session_ctrl", "action":"stop"}
{"type":"session_ctrl", "action":"cancel", "session_id":"uuid-xxx"}
{"type":"session_ctrl", "action":"reconnect"}
```

| action | 说明 | session_id |
|---|---|---|
| `new` | 创建新会话 | 可选 |
| `cancel` | 取消当前请求 | 可选 |
| `list` | 列出会话 | 不需要 |
| `switch` | 切换会话 | 必须 |
| `stop` | 结束对话（优雅关闭 CC 进程）| 不需要 |
| `reconnect` | 重新连接 | 不需要 |

### 4.3 auth_response — 授权回复

```json
{"type":"auth_response", "request_id":"req_01XYZ", "decision":"allow"}
{"type":"auth_response", "request_id":"req_01XYZ", "decision":"deny"}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| type | string | `"auth_response"` |
| request_id | string | 关联的授权请求 ID |
| decision | string | `"allow"` 或 `"deny"` |

### 4.4 audio — 音频数据（二进制帧）

音频数据通过 **WebSocket 二进制帧**（opcode 0x02）直接发送，无 JSON 包装。

```
[WebSocket Binary Frame]
  opcode: 0x02
  payload: Opus encoded audio data
```

| 参数 | 值 |
|---|---|
| 编码 | Opus |
| 采样率 | 24000 Hz |
| 位深 | 16 bit |
| 声道 | 单声道 |
| 帧时长 | 20ms |
| 发送方式 | `esp_websocket_client_send_bin()` |

---

## 5. 心跳机制

### 5.1 WebSocket 内置 ping/pong

```
T=0       客户端发送 WS ping（esp_websocket_client 自动）
T=30s     客户端发送 WS ping
T=60s     客户端发送 WS ping
T=60+s    如 60s 内未收到 pong → 连接超时 → 触发重连
```

**配置**：
- `ping_interval_sec = 30`
- `pingpong_timeout_sec = 60`

### 5.2 应用层心跳（可选扩展）

P4C5 可新增 `status_report` 上行帧（P4C5 特有，OMT 无此帧）：

```json
{
  "type": "status_report",
  "battery": 87,
  "charging": true,
  "vbat_mv": 4050,
  "die_temp": 38,
  "rssi": -75,
  "network": "wifi",
  "free_heap": 125000,
  "uptime_sec": 3600
}
```

> ⚠️ 此帧为 P4C5 扩展，DSH Adapter 需适配解析。如不兼容，可降级为 text 帧发送。

---

## 6. 多包拼接机制

### 6.1 问题背景

ESP-IDF `esp_websocket_client` 当 payload > buffer_size (4096) 时，会将一个完整 JSON 拆分为多个 `WEBSOCKET_EVENT_DATA` 事件。

**实测案例**（OMT 2026-08-30）：
- assistant_text 实际 payload > 4096 字节
- 被拆分为 4 个事件：1326 + 1330 + 695 + 498 = 3849 字节（实际 total 更大）

### 6.2 拼接算法

```c
// OMT dsh_client.cpp 实现（关键逻辑）
if (data->payload_len > 0 && data->payload_offset >= 0) {
    // 多包模式
    if (s_ws_rx_buffer_len != data->payload_offset) {
        // offset 不连续 → 丢弃旧 buffer（新 payload 开始）
        s_ws_rx_buffer_len = 0;
    }
    // 追加到拼接 buffer（动态 realloc）
    memcpy(s_ws_rx_buffer + s_ws_rx_buffer_len, data->data_ptr, data->data_len);
    s_ws_rx_buffer_len += data->data_len;
    
    if (s_ws_rx_buffer_len == data->payload_len) {
        // 完整 payload 接收完毕 → 一次性解析
        parse_and_dispatch_event(s_ws_rx_buffer, s_ws_rx_buffer_len);
        s_ws_rx_buffer_len = 0;
    }
} else {
    // 单包模式（payload ≤ buffer_size）→ 直接解析
    parse_and_dispatch_event(data->data_ptr, data->data_len);
}
```

### 6.3 buffer 管理

| 参数 | 值 |
|---|---|
| 初始容量 | 16384 bytes |
| 扩容策略 | `cap *= 2` 直到足够 |
| 释放时机 | 每次完整 payload 解析后（len 归零，buffer 保留）|

---

## 7. 帧编解码参考

### 7.1 JSON 编码（上行帧）

```c
// 发送 text 帧
cJSON *root = cJSON_CreateObject();
cJSON_AddStringToObject(root, "type", "text");
cJSON_AddStringToObject(root, "content", user_input);
char *json_str = cJSON_PrintUnformatted(root);
esp_websocket_client_send_text(ws_client, json_str, strlen(json_str), pdMS_TO_TICKS(5000));
cJSON_Delete(root);
free(json_str);
```

### 7.2 JSON 解码（下行帧）

```c
// 解析下行帧
cJSON *root = cJSON_ParseWithLength(json, len);
cJSON *type_item = cJSON_GetObjectItem(root, "type");
if (strcmp(type_item->valuestring, "assistant_text") == 0) {
    cJSON *content = cJSON_GetObjectItem(root, "content");
    // 处理 assistant_text 事件
}
cJSON_Delete(root);
```

### 7.3 关键注意事项

1. **buffer_size = 4096**：不可增大到 16384，否则 `esp_websocket_client_destroy` 后 reconnect_task 创建失败（Timer Svc 2KB 栈不够）
2. **task_stack = 8192**：WS 事件处理任务需要足够栈空间
3. **发送超时 = 5000ms**：`esp_websocket_client_send_text` 的默认超时
4. **stop_conversation 特殊超时 = 200ms**：避免 cJSON 分配导致栈溢出蓝屏
5. **防重入**：`stop_conversation` 使用 `volatile bool` 防止并发调用

---

## 8. P4C5 扩展帧（vs OMT 新增）

| 帧名 | 方向 | 说明 |
|---|---|---|
| `status_report` | C→S | 电池/信号/温度/内存上报 |
| `touch_event` | C→S | 触摸坐标上报（替代 DSH 直接控制）|
| `key_event` | C→S | 按键事件上报 |
| `display_ready` | C→S | Display 初始化完成通知 |

> ⚠️ 扩展帧需 DSH Adapter 侧同步适配。建议先用 text 帧封装测试，验证通过后再定义独立 type。

---

**版本**：v1.0（2026-09-06, CCB）
**数据来源**：OMT `dsh_client.cpp` 1141 行 + `event_bus.h` 80 行 + `dsh_client.h` 98 行
**下次更新**：CCA T8 实现完成后，补充 P4C5 特有帧定义
