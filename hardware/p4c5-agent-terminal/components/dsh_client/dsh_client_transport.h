/**
 * @file dsh_client_transport.h
 * @brief dsh_client 传输层抽象接口
 *
 * 允许 dsh_client 使用不同的 WebSocket 实现：
 * - esp_websocket_client（WiFi 场景）
 * - esp-ml307 WebSocket（4G 场景）
 */

#pragma once

#include "cJSON.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 传输层回调 ── */

/** 连接状态变化回调 */
typedef void (*dsh_transport_state_cb_t)(bool connected, void *user_data);

/**
 * 收到 JSON 帧回调
 *
 * @param json_text  原始 JSON 字符串
 * @param frame_type JSON "type" 字段值
 * @param json       解析后的 cJSON 对象
 * @param user_data  用户数据
 */
typedef void (*dsh_transport_json_cb_t)(const char *json_text,
                                         const char *frame_type,
                                         cJSON *json,
                                         void *user_data);

/* ── 传输层 vtable ── */

typedef struct {
    const char *name;

    esp_err_t (*init)(const char *uri, uint32_t buffer_size);
    void      (*deinit)(void);
    esp_err_t (*start)(void);
    esp_err_t (*stop)(void);
    esp_err_t (*send_text)(const char *text, size_t len);
    esp_err_t (*send_binary)(const uint8_t *data, size_t len);  /* W3: 音频上行 */
    bool      (*is_connected)(void);

    void (*set_state_cb)(dsh_transport_state_cb_t cb, void *user_data);
    void (*set_json_cb)(dsh_transport_json_cb_t cb, void *user_data);
} dsh_transport_t;

/**
 * 获取 esp_websocket_client 传输层（WiFi/以太网）
 */
const dsh_transport_t *dsh_transport_get_ws(void);

/**
 * 获取 ML307 WebSocket 传输层（4G）
 * 如果 p4c5_4g 未初始化或网络未就绪，返回 NULL
 */
const dsh_transport_t *dsh_transport_get_ml307(void);

#ifdef __cplusplus
}
#endif
