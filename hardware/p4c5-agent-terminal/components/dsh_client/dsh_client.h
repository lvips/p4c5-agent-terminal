/**
 * @file dsh_client.h
 * @brief DSH WebSocket 客户端协议层 — 公开 API
 *
 * JSON-over-WebSocket 协议，支持 18 类帧（14 下行 + 4 上行）。
 * 基于 CCB T8 调研结果实现。
 *
 * 参考文档：docs/hw/dsh-client-frame-protocol.md
 *
 * 使用示例：
 * @code
 *   dsh_client_config_t cfg = {
 *       .url = "ws://192.168.1.100:8080/ws",
 *       .device_id = "p4c5-001",
 *       .auth_token = "secret",
 *   };
 *   dsh_client_init(&cfg);
 *   dsh_client_register_event_callback(my_handler, NULL);
 *   dsh_client_connect();
 * @endcode
 */

#pragma once

#include "cJSON.h"
#include "esp_err.h"
#include "esp_netif.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 版本号 ── */
#define DSH_CLIENT_VERSION  "0.1.0"

/* ══════════════════════════════════════════════════════════
 * 配置 + 类型定义
 * ══════════════════════════════════════════════════════════ */

/**
 * 客户端配置
 */
typedef struct {
    const char *url;            /**< WebSocket URL (ws://host:port/ws) */
    const char *device_id;      /**< 设备标识 */
    const char *auth_token;     /**< 认证令牌（可为 NULL） */
    bool use_tls;               /**< 是否使用 WSS（暂不支持） */
    uint32_t ws_buffer_size;    /**< WS 接收缓冲区（0 = 默认 4096） */
} dsh_client_config_t;

/**
 * 客户端事件类型
 */
typedef enum {
    DSH_EVENT_CONNECTED,        /**< WebSocket 连接成功 + HELLO 已发送 */
    DSH_EVENT_DISCONNECTED,     /**< WebSocket 断开 */
    DSH_EVENT_ERROR,            /**< 传输层错误 */
    DSH_EVENT_RECONNECTING,     /**< 正在重连 */
} dsh_client_event_t;

/**
 * 帧接收回调
 *
 * @param frame_type  JSON "type" 字段值（如 "assistant_text"）
 * @param payload     解析后的完整 JSON 对象（只读，不要 cJSON_Delete）
 * @param user_data   注册时传入的用户数据
 */
typedef void (*dsh_client_event_cb_t)(const char *frame_type,
                                       cJSON *payload,
                                       void *user_data);

/**
 * 连接状态回调
 */
typedef void (*dsh_client_state_cb_t)(dsh_client_event_t event, void *user_data);

/**
 * 状态查询回调 — 用于心跳帧中携带电池/信号数据
 */
typedef void (*dsh_client_status_cb_t)(int *battery_pct, int *rssi_dbm, void *user_data);

/* ══════════════════════════════════════════════════════════
 * 生命周期 API（4 个）
 * ══════════════════════════════════════════════════════════ */

/**
 * 初始化 DSH 客户端
 *
 * 复制配置参数，创建内部资源。调用一次即可。
 *
 * @param config 配置（url 必填）
 * @return ESP_OK / ESP_ERR_INVALID_ARG / ESP_ERR_NO_MEM
 */
esp_err_t dsh_client_init(const dsh_client_config_t *config);

/**
 * 连接到 DSH 服务端
 *
 * 启动 WebSocket 连接 → 连接成功后自动发送 client/hello → 启动心跳。
 * 断线后自动重连（指数退避 1s → 2s → 4s → ... → 60s max）。
 *
 * @return ESP_OK / ESP_ERR_INVALID_STATE / ESP_FAIL
 */
esp_err_t dsh_client_connect(void);

/**
 * 断开连接
 *
 * 停止心跳 + 关闭 WebSocket + 停止重连任务。
 */
esp_err_t dsh_client_disconnect(void);

/**
 * 释放所有资源
 *
 * 调用后必须重新 dsh_client_init() 才能使用。
 */
void dsh_client_deinit(void);

/* ══════════════════════════════════════════════════════════
 * 状态查询（1 个）
 * ══════════════════════════════════════════════════════════ */

/**
 * 是否已连接（WebSocket 已建立 + HELLO 已发送）
 */
bool dsh_client_is_connected(void);

/* ══════════════════════════════════════════════════════════
 * 回调注册（2 个）
 * ══════════════════════════════════════════════════════════ */

/**
 * 注册帧接收回调
 *
 * 收到任何下行 JSON 帧时调用。回调中根据 frame_type 分发处理。
 * 回调在 WebSocket 事件任务上下文中执行，不要长时间阻塞。
 *
 * @param cb        回调函数
 * @param user_data 用户数据
 */
esp_err_t dsh_client_register_event_callback(dsh_client_event_cb_t cb, void *user_data);

/**
 * 注册连接状态回调
 */
esp_err_t dsh_client_register_state_callback(dsh_client_state_cb_t cb, void *user_data);

/* ══════════════════════════════════════════════════════════
 * 发送上行帧（4 个）
 * ══════════════════════════════════════════════════════════ */

/**
 * 发送 client/hello
 *
 * 连接成功后自动发送，通常不需要手动调用。
 */
esp_err_t dsh_client_send_hello(void);

/**
 * 发送 client/heartbeat
 *
 * 通常由心跳任务自动发送。手动调用可用于测试。
 */
esp_err_t dsh_client_send_heartbeat(void);

/**
 * 发送 client/tool_result
 *
 * @param tool_id   工具调用 ID（来自 tool_call 帧的 "id"）
 * @param result    执行结果 JSON（可为 NULL）
 */
esp_err_t dsh_client_send_tool_result(const char *tool_id, cJSON *result);

/**
 * 发送 client/user_input
 *
 * @param text 用户输入文本
 */
esp_err_t dsh_client_send_user_input(const char *text);

/* ══════════════════════════════════════════════════════════
 * 通用发送（1 个）
 * ══════════════════════════════════════════════════════════ */

/**
 * 发送自定义帧
 *
 * @param frame_type  帧类型字符串
 * @param payload     附加 JSON 字段（cJSON object，可为 NULL）
 */
esp_err_t dsh_client_send(const char *frame_type, cJSON *payload);

/* ══════════════════════════════════════════════════════════
 * 心跳控制（2 个）
 * ══════════════════════════════════════════════════════════ */

/**
 * 启动自动心跳（默认 30s 间隔）
 *
 * connect() 时自动调用。手动调用可覆盖间隔。
 */
esp_err_t dsh_client_start_heartbeat(uint32_t interval_ms);

/**
 * 停止自动心跳
 */
esp_err_t dsh_client_stop_heartbeat(void);

/* ══════════════════════════════════════════════════════════
 * 网络接口（1 个）
 * ══════════════════════════════════════════════════════════ */

/**
 * 绑定网络接口
 *
 * 在 connect() 之前调用。用于 4G/WiFi 切换场景。
 *
 * @param netif  esp_netif 句柄（4G 或 WiFi 的 netif）
 */
esp_err_t dsh_client_attach_netif(esp_netif_t *netif);

/* ══════════════════════════════════════════════════════════
 * 可选：状态数据提供者（1 个）
 * ══════════════════════════════════════════════════════════ */

/**
 * 注册状态数据提供者
 *
 * 心跳帧需要电池电量和信号强度。注册此回调后，心跳帧
 * 会自动查询并携带这些数据。不注册则 battery=-1, rssi=0。
 */
esp_err_t dsh_client_set_status_provider(dsh_client_status_cb_t cb, void *user_data);

#ifdef __cplusplus
}
#endif
