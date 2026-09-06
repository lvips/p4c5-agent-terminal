/**
 * @file dsh_client_ws.c
 * @brief WebSocket 传输层实现
 *
 * 基于 esp_websocket_client (~1.1.0)，处理连接/断开/收发。
 * 收到 TEXT 帧后解析 JSON，提取 "type"，调用上层回调。
 * 断线后利用内置 auto_reconnect 机制重连。
 */

#include "dsh_client_ws.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "esp_transport_ws.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "dsh_ws";

/* ── 内部状态 ── */
static esp_websocket_client_handle_t s_ws = NULL;
static dsh_ws_json_cb_t   s_json_cb      = NULL;
static void              *s_json_cb_data = NULL;
static dsh_ws_binary_cb_t s_binary_cb      = NULL;  /* W3: TTS 下行 PCM */
static void              *s_binary_cb_data = NULL;
static dsh_ws_state_cb_t  s_state_cb      = NULL;
static void              *s_state_cb_data = NULL;
static bool              s_connected    = false;
static bool              s_running      = false;
static char             *s_uri          = NULL;
static uint32_t          s_buffer_size  = 4096;

/* ══════════════════════════════════════════════════════════
 * WebSocket 事件处理
 * ══════════════════════════════════════════════════════════ */

static void ws_event_handler(void *handler_args, esp_event_base_t base,
                              int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WebSocket connected");
        s_connected = true;
        if (s_state_cb) {
            s_state_cb(true, s_state_cb_data);
        }
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "WebSocket disconnected");
        s_connected = false;
        if (s_state_cb) {
            s_state_cb(false, s_state_cb_data);
        }
        break;

    case WEBSOCKET_EVENT_DATA: {
        /* 只处理 TEXT 帧（opcode 0x01） */
        if (data->op_code == WS_TRANSPORT_OPCODES_TEXT && data->data_len > 0) {
            /* data_ptr 不一定 null-terminated，拷贝后加 '\0' */
            char *json_text = (char *)malloc(data->data_len + 1);
            if (json_text) {
                memcpy(json_text, data->data_ptr, data->data_len);
                json_text[data->data_len] = '\0';

                ESP_LOGD(TAG, "RX TEXT (%d): %s", data->data_len, json_text);

                /* 解析 JSON + 提取 type */
                cJSON *json = cJSON_Parse(json_text);
                if (json) {
                    cJSON *type_item = cJSON_GetObjectItemCaseSensitive(json, "type");
                    const char *frame_type = (cJSON_IsString(type_item))
                                              ? type_item->valuestring : "unknown";

                    if (s_json_cb) {
                        s_json_cb(json_text, frame_type, json, s_json_cb_data);
                    }

                    cJSON_Delete(json);
                } else {
                    ESP_LOGW(TAG, "JSON parse failed: %.128s", json_text);
                }

                free(json_text);
            }
        } else if (data->op_code == WS_TRANSPORT_OPCODES_BINARY) {
            /* BINARY 帧 — W3: 上行可能用于 opus 音频, 下行用于 TTS PCM */
            if (s_binary_cb && data->data_len > 0) {
                s_binary_cb((const uint8_t *)data->data_ptr, data->data_len, s_binary_cb_data);
            }
        }
        break;
    }

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WebSocket error");
        if (s_state_cb) {
            s_state_cb(false, s_state_cb_data);
        }
        break;

    default:
        break;
    }
}

/* ══════════════════════════════════════════════════════════
 * 公开接口
 * ══════════════════════════════════════════════════════════ */

esp_err_t dsh_ws_init(const char *uri, uint32_t buffer_size)
{
    if (!uri) return ESP_ERR_INVALID_ARG;
    if (s_ws) return ESP_ERR_INVALID_STATE;

    s_uri = strdup(uri);
    if (!s_uri) return ESP_ERR_NO_MEM;

    s_buffer_size = (buffer_size > 0) ? buffer_size : 4096;
    s_running = false;
    s_connected = false;

    ESP_LOGI(TAG, "WS transport init: uri=%s, buf=%lu", s_uri, (unsigned long)s_buffer_size);
    return ESP_OK;
}

void dsh_ws_set_json_callback(dsh_ws_json_cb_t cb, void *user_data)
{
    s_json_cb = cb;
    s_json_cb_data = user_data;
}

void dsh_ws_set_binary_callback(dsh_ws_binary_cb_t cb, void *user_data)
{
    s_binary_cb = cb;
    s_binary_cb_data = user_data;
}

void dsh_ws_set_state_callback(dsh_ws_state_cb_t cb, void *user_data)
{
    s_state_cb = cb;
    s_state_cb_data = user_data;
}

esp_err_t dsh_ws_attach_netif(esp_netif_t *netif)
{
    /* 在 esp_websocket_client ~1.1.0 中，if_name 是 struct ifreq*
     * 不支持直接绑定 esp_netif_t*。暂存但不使用。
     * TODO: 在更高版本中使用 esp_transport_set_if_name() */
    ESP_LOGI(TAG, "Netif attach noted: %p (not bound in this API version)", netif);
    return ESP_OK;
}

esp_err_t dsh_ws_start(void)
{
    if (!s_uri) return ESP_ERR_INVALID_STATE;
    if (s_ws) return ESP_ERR_INVALID_STATE;

    ESP_LOGI(TAG, "Starting WebSocket client...");

    esp_websocket_client_config_t ws_cfg = {
        .uri = s_uri,
        .buffer_size = s_buffer_size,
        /* 启用内置自动重连（默认 10s 间隔） */
        .disable_auto_reconnect = false,
        .reconnect_timeout_ms = 10000,
        .network_timeout_ms = 30000,
        .ping_interval_sec = 30,      /* 协议层 ping 保活 */
    };

    s_ws = esp_websocket_client_init(&ws_cfg);
    if (!s_ws) {
        ESP_LOGE(TAG, "Failed to create WebSocket client");
        return ESP_FAIL;
    }

    esp_websocket_register_events(s_ws, WEBSOCKET_EVENT_ANY, ws_event_handler, NULL);

    s_running = true;

    esp_err_t err = esp_websocket_client_start(s_ws);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start: %s", esp_err_to_name(err));
        esp_websocket_client_destroy(s_ws);
        s_ws = NULL;
        s_running = false;
        return err;
    }

    ESP_LOGI(TAG, "WebSocket client started");
    return ESP_OK;
}

esp_err_t dsh_ws_stop(void)
{
    s_running = false;

    if (s_ws) {
        esp_websocket_client_close(s_ws, pdMS_TO_TICKS(3000));
        esp_websocket_client_destroy(s_ws);
        s_ws = NULL;
    }

    s_connected = false;
    ESP_LOGI(TAG, "WebSocket stopped");
    return ESP_OK;
}

void dsh_ws_deinit(void)
{
    dsh_ws_stop();

    free(s_uri);
    s_uri = NULL;
    s_json_cb = NULL;
    s_state_cb = NULL;

    ESP_LOGI(TAG, "WS transport deinitialized");
}

esp_err_t dsh_ws_send_json(cJSON *json)
{
    if (!s_ws || !s_connected) {
        ESP_LOGW(TAG, "Not connected, cannot send");
        return ESP_ERR_INVALID_STATE;
    }
    if (!json) return ESP_ERR_INVALID_ARG;

    char *text = cJSON_PrintUnformatted(json);
    if (!text) return ESP_ERR_NO_MEM;

    int len = (int)strlen(text);
    ESP_LOGD(TAG, "TX TEXT (%d): %s", len, text);

    /* 使用 send_text（opcode=0x01 TEXT 帧） */
    int ret = esp_websocket_client_send_text(
        s_ws, text, len, pdMS_TO_TICKS(5000));

    free(text);

    if (ret < 0) {
        ESP_LOGE(TAG, "Send failed: %d", ret);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t dsh_ws_send_text(const char *text)
{
    if (!s_ws || !s_connected) {
        ESP_LOGW(TAG, "Not connected, cannot send");
        return ESP_ERR_INVALID_STATE;
    }
    if (!text) return ESP_ERR_INVALID_ARG;

    int len = (int)strlen(text);
    int ret = esp_websocket_client_send_text(
        s_ws, text, len, pdMS_TO_TICKS(5000));

    return (ret >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t dsh_ws_send_binary(const uint8_t *data, size_t len)
{
    if (!s_ws || !s_connected) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;

    /* WS Binary 帧上行 (W3: Opus 音频帧) */
    int ret = esp_websocket_client_send_bin(
        s_ws, (const char *)data, (int)len, pdMS_TO_TICKS(5000));

    return (ret >= 0) ? ESP_OK : ESP_FAIL;
}

bool dsh_ws_is_connected(void)
{
    return s_connected;
}
