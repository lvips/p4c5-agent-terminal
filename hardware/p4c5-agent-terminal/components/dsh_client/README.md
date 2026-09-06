# dsh_client — DSH JSON-over-WebSocket 协议层

## 功能

实现与 DSH (DeepSeek Harness) 服务端的 JSON-over-WebSocket 通信协议。
基于 CCB T8 调研结果，与 OMT 项目协议兼容。

## 协议概述

所有帧均为 JSON TEXT 格式通过 WebSocket 传输，用 `{"type":"xxx"}` 区分帧类型。

### 下行帧（服务端→客户端，14 类）

| 帧类型 | 说明 |
|--------|------|
| `session_state` | 会话状态变化 |
| `thinking` | 思考中指示 |
| `assistant_text` | 流式文本增量 |
| `assistant_done` | 回复完成 |
| `tool_call` | 工具调用请求 |
| `tool_result` | 工具结果（服务端中继） |
| `tool_progress` | 工具执行进度 |
| `file_change` | 文件变更通知 |
| `authorization/requested` | 授权请求 |
| `authorization/resolved` | 授权完成 |
| `question/requested` | 问题请求 |
| `question/resolved` | 问题已回答 |
| `file_upload` | 文件上传 |
| `system_event` | 系统事件 |

### 上行帧（客户端→服务端，4 类）

| 帧类型 | 说明 |
|--------|------|
| `client/hello` | 注册/认证 |
| `client/heartbeat` | 心跳保活（30s） |
| `client/tool_result` | 工具执行结果 |
| `client/user_input` | 用户输入 |

## API（13 个）

```c
// 生命周期（4）
esp_err_t dsh_client_init(const dsh_client_config_t *config);
esp_err_t dsh_client_connect(void);
esp_err_t dsh_client_disconnect(void);
void      dsh_client_deinit(void);

// 状态（1）
bool dsh_client_is_connected(void);

// 回调注册（2）
esp_err_t dsh_client_register_event_callback(dsh_client_event_cb_t cb, void *user_data);
esp_err_t dsh_client_register_state_callback(dsh_client_state_cb_t cb, void *user_data);

// 发送上行帧（4）
esp_err_t dsh_client_send_hello(void);
esp_err_t dsh_client_send_heartbeat(void);
esp_err_t dsh_client_send_tool_result(const char *tool_id, cJSON *result);
esp_err_t dsh_client_send_user_input(const char *text);

// 通用发送（1）
esp_err_t dsh_client_send(const char *frame_type, cJSON *payload);

// 心跳控制（2）→ 但 connect() 自动启动
esp_err_t dsh_client_start_heartbeat(uint32_t interval_ms);
esp_err_t dsh_client_stop_heartbeat(void);

// 网络接口（1）
esp_err_t dsh_client_attach_netif(esp_netif_t *netif);

// 状态提供者（1）→ 心跳帧携带电池/信号数据
esp_err_t dsh_client_set_status_provider(dsh_client_status_cb_t cb, void *user_data);
```

## 文件结构

```
components/dsh_client/
├── dsh_client.h            # 公开 API（13 个函数）
├── dsh_client.c            # 主控制逻辑
├── dsh_client_ws.h/c       # WebSocket 传输层（自动重连）
├── dsh_client_frames.h/c   # 帧类型常量 + JSON 构造/解析
├── dsh_client_heartbeat.h/c # 心跳任务
├── CMakeLists.txt
├── idf_component.yml
└── README.md
```

## 使用示例

```c
#include "dsh_client.h"

// 帧接收回调
void on_frame(const char *type, cJSON *json, void *ud) {
    if (strcmp(type, "assistant_text") == 0) {
        cJSON *delta = cJSON_GetObjectItem(json, "delta");
        if (delta) printf("%s", delta->valuestring);
    } else if (strcmp(type, "tool_call") == 0) {
        cJSON *name = cJSON_GetObjectItem(json, "name");
        cJSON *id   = cJSON_GetObjectItem(json, "id");
        // 执行工具...
        dsh_client_send_tool_result(id->valuestring, result_json);
    }
}

// 初始化
dsh_client_config_t cfg = {
    .url = "ws://192.168.1.100:8080/ws",
    .device_id = "p4c5-001",
    .auth_token = "secret",
};
dsh_client_init(&cfg);
dsh_client_register_event_callback(on_frame, NULL);
dsh_client_connect();
```

## 依赖

- `espressif/esp_websocket_client` ~1.1.0
- `json`（IDF 自带 cJSON）
- IDF >= 5.4.0

## 设计说明

- **自动重连**：指数退避（1s → 2s → 4s → ... → 60s max）
- **心跳**：FreeRTOS 独立任务，30s 间隔，携带电池/信号
- **零硬件依赖**：不直接依赖 p4c5_pmic / p4c5_4g，通过回调获取状态数据
- **线程安全**：回调在 WebSocket 事件任务上下文执行，不要长时间阻塞

## 状态

- ✅ 骨架实现完成（8 文件，13 API，18 帧类型）
- ⚠️ 未连接真实 DSH 服务端测试
- ⚠️ TLS (WSS) 未实现
- ⚠️ BINARY 帧（音频流）未处理
