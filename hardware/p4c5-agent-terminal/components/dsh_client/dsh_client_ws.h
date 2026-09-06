/**
 * @file dsh_client_ws.h
 * @brief WebSocket 传输层内部接口
 *
 * 内部头文件 — 仅 dsh_client 组件内部使用
 */

#pragma once

#include "cJSON.h"
#include "esp_err.h"
#include "esp_netif.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 收到 JSON TEXT 帧的回调
 *
 * @param json_text 原始 JSON 字符串（帧生命周期内有效）
 * @param frame_type "type" 字段值（指向 json_text 内部，不要 free）
 * @param json       解析后的 cJSON 对象（不要 cJSON_Delete，由调用方管理）
 * @param user_data  用户数据
 */
typedef void (*dsh_ws_json_cb_t)(const char *json_text,
                                  const char *frame_type,
                                  cJSON *json,
                                  void *user_data);

/**
 * 连接状态变化回调
 */
typedef void (*dsh_ws_state_cb_t)(bool connected, void *user_data);

/**
 * 初始化 WebSocket 传输层
 */
esp_err_t dsh_ws_init(const char *uri, uint32_t buffer_size);

/**
 * 设置接收 JSON 帧的回调
 */
void dsh_ws_set_json_callback(dsh_ws_json_cb_t cb, void *user_data);

/**
 * 设置连接状态回调
 */
void dsh_ws_set_state_callback(dsh_ws_state_cb_t cb, void *user_data);

/**
 * 绑定网络接口（4G 或 WiFi）
 */
esp_err_t dsh_ws_attach_netif(esp_netif_t *netif);

/**
 * 启动 WebSocket 连接
 */
esp_err_t dsh_ws_start(void);

/**
 * 停止 WebSocket 连接（不销毁句柄，可重连）
 */
esp_err_t dsh_ws_stop(void);

/**
 * 销毁 WebSocket 传输层
 */
void dsh_ws_deinit(void);

/**
 * 发送 JSON 对象（序列化为 TEXT 帧发送）
 *
 * @param json cJSON 对象，发送后 cJSON_Delete()
 * @return ESP_OK 成功
 */
esp_err_t dsh_ws_send_json(cJSON *json);

/**
 * 发送原始 JSON 字符串
 */
esp_err_t dsh_ws_send_text(const char *text);

/**
 * 发送二进制帧 (W3: 音频上行 Opus 帧)
 *
 * 用于实时音频流上传 (Opus 编码后的 PCM 帧, 16kbps, 20ms 帧)。
 * 实际是 WS Binary 帧 (per RFC 6455)。
 *
 * @param data 二进制数据
 * @param len  数据长度 (字节)
 * @return ESP_OK 成功, ESP_ERR_INVALID_STATE 未连接
 */
esp_err_t dsh_ws_send_binary(const uint8_t *data, size_t len);

/**
 * 是否已连接
 */
bool dsh_ws_is_connected(void);

#ifdef __cplusplus
}
#endif
