/**
 * @file dsh_client.c
 * @brief DSH 客户端主控制逻辑
 *
 * 通过 transport 抽象层支持多种 WebSocket 后端：
 * - esp_websocket_client（WiFi，默认）
 * - esp-ml307 WebSocket（4G，通过 dsh_client_set_transport()）
 */

#include "dsh_client.h"
#include "dsh_client_transport.h"
#include "dsh_client_frames.h"
#include "dsh_client_heartbeat.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "dsh_client";

/* ══════════════════════════════════════════════════════════
 * 内部状态
 * ══════════════════════════════════════════════════════════ */

static bool s_initialized = false;
static bool s_connected   = false;

/* 配置（深拷贝字符串） */
static struct {
    char *url;
    char *device_id;
    char *auth_token;
    bool  use_tls;
    uint32_t ws_buffer_size;
} s_cfg = {0};

/* 传输层 */
static const dsh_transport_t *s_transport = NULL;

/* 回调 */
static dsh_client_event_cb_t  s_frame_cb       = NULL;
static void                  *s_frame_cb_data   = NULL;
static dsh_client_state_cb_t  s_state_cb        = NULL;
static void                  *s_state_cb_data    = NULL;
static dsh_client_status_cb_t s_status_cb       = NULL;
static void                  *s_status_cb_data   = NULL;

/* ══════════════════════════════════════════════════════════
 * 内部回调桥接
 * ══════════════════════════════════════════════════════════ */

static void on_transport_json(const char *json_text, const char *frame_type,
                               cJSON *json, void *user_data)
{
    ESP_LOGD(TAG, "Frame RX: type=%s", frame_type ? frame_type : "null");
    if (s_frame_cb) {
        s_frame_cb(frame_type, json, s_frame_cb_data);
    }
}

static void on_transport_state(bool connected, void *user_data)
{
    s_connected = connected;

    if (connected) {
        ESP_LOGI(TAG, "Connected → sending HELLO");
        dsh_client_send_hello();
        if (s_state_cb) {
            s_state_cb(DSH_EVENT_CONNECTED, s_state_cb_data);
        }
    } else {
        ESP_LOGW(TAG, "Disconnected");
        dsh_heartbeat_stop();
        if (s_state_cb) {
            s_state_cb(DSH_EVENT_DISCONNECTED, s_state_cb_data);
        }
    }
}

static void heartbeat_status_provider(int *battery, int *rssi, void *ud)
{
    if (s_status_cb) {
        s_status_cb(battery, rssi, s_status_cb_data);
    } else {
        *battery = -1;
        *rssi = 0;
    }
}

/* ══════════════════════════════════════════════════════════
 * 生命周期 API
 * ══════════════════════════════════════════════════════════ */

esp_err_t dsh_client_init(const dsh_client_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    if (!config || !config->url) {
        ESP_LOGE(TAG, "config/url is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    /* 深拷贝配置 */
    s_cfg.url = strdup(config->url);
    s_cfg.device_id = config->device_id ? strdup(config->device_id) : strdup("unknown");
    s_cfg.auth_token = config->auth_token ? strdup(config->auth_token) : NULL;
    s_cfg.use_tls = config->use_tls;
    s_cfg.ws_buffer_size = config->ws_buffer_size;

    if (!s_cfg.url || !s_cfg.device_id) {
        ESP_LOGE(TAG, "Out of memory");
        free(s_cfg.url);
        free(s_cfg.device_id);
        free(s_cfg.auth_token);
        return ESP_ERR_NO_MEM;
    }

    /* 选择传输层 */
    if (!s_transport) {
        /* 默认使用 esp_websocket_client（WiFi） */
        s_transport = dsh_transport_get_ws();
    }

    /* 初始化传输层 */
    if (s_transport && s_transport->init) {
        esp_err_t err = s_transport->init(s_cfg.url, s_cfg.ws_buffer_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Transport init failed: %s", esp_err_to_name(err));
            return err;
        }
    }

    /* 注册内部回调 */
    if (s_transport && s_transport->set_json_cb) {
        s_transport->set_json_cb(on_transport_json, NULL);
    }
    if (s_transport && s_transport->set_state_cb) {
        s_transport->set_state_cb(on_transport_state, NULL);
    }

    s_initialized = true;
    ESP_LOGI(TAG, "DSH client init: url=%s, device=%s, transport=%s",
             s_cfg.url, s_cfg.device_id,
             s_transport ? s_transport->name : "none");
    return ESP_OK;
}

esp_err_t dsh_client_connect(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (s_connected) return ESP_OK;

    ESP_LOGI(TAG, "Connecting to %s...", s_cfg.url);

    if (s_transport && s_transport->start) {
        esp_err_t err = s_transport->start();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Transport start failed: %s", esp_err_to_name(err));
            return err;
        }
    }

    /* 启动心跳（默认 30s） */
    dsh_client_start_heartbeat(30000);
    return ESP_OK;
}

esp_err_t dsh_client_disconnect(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    dsh_heartbeat_stop();
    if (s_transport && s_transport->stop) {
        s_transport->stop();
    }
    s_connected = false;
    ESP_LOGI(TAG, "Disconnected");
    return ESP_OK;
}

void dsh_client_deinit(void)
{
    if (!s_initialized) return;

    dsh_client_disconnect();
    if (s_transport && s_transport->deinit) {
        s_transport->deinit();
    }

    free(s_cfg.url);
    free(s_cfg.device_id);
    free(s_cfg.auth_token);
    memset(&s_cfg, 0, sizeof(s_cfg));

    s_frame_cb = NULL;
    s_state_cb = NULL;
    s_status_cb = NULL;
    s_transport = NULL;
    s_initialized = false;
    ESP_LOGI(TAG, "Deinitialized");
}

/* ══════════════════════════════════════════════════════════
 * 状态查询
 * ══════════════════════════════════════════════════════════ */

bool dsh_client_is_connected(void)
{
    return s_connected;
}

/* ══════════════════════════════════════════════════════════
 * 回调注册
 * ══════════════════════════════════════════════════════════ */

esp_err_t dsh_client_register_event_callback(dsh_client_event_cb_t cb, void *user_data)
{
    s_frame_cb = cb;
    s_frame_cb_data = user_data;
    return ESP_OK;
}

esp_err_t dsh_client_register_state_callback(dsh_client_state_cb_t cb, void *user_data)
{
    s_state_cb = cb;
    s_state_cb_data = user_data;
    return ESP_OK;
}

/* ══════════════════════════════════════════════════════════
 * 发送上行帧
 * ══════════════════════════════════════════════════════════ */

esp_err_t dsh_client_send_hello(void)
{
    cJSON *hello = dsh_frame_build_hello(
        s_cfg.device_id, DSH_CLIENT_VERSION, NULL, s_cfg.auth_token);
    if (!hello) return ESP_ERR_NO_MEM;

    char *text = cJSON_PrintUnformatted(hello);
    cJSON_Delete(hello);
    if (!text) return ESP_ERR_NO_MEM;

    esp_err_t err = ESP_OK;
    if (s_transport && s_transport->send_text) {
        err = s_transport->send_text(text, strlen(text));
    } else {
        err = ESP_ERR_NOT_SUPPORTED;
    }
    free(text);
    return err;
}

esp_err_t dsh_client_send_heartbeat(void)
{
    int battery = -1, rssi = 0;
    if (s_status_cb) {
        s_status_cb(&battery, &rssi, s_status_cb_data);
    }

    cJSON *hb = dsh_frame_build_heartbeat(battery, rssi);
    if (!hb) return ESP_ERR_NO_MEM;

    char *text = cJSON_PrintUnformatted(hb);
    cJSON_Delete(hb);
    if (!text) return ESP_ERR_NO_MEM;

    esp_err_t err = ESP_OK;
    if (s_transport && s_transport->send_text) {
        err = s_transport->send_text(text, strlen(text));
    } else {
        err = ESP_ERR_NOT_SUPPORTED;
    }
    free(text);
    return err;
}

esp_err_t dsh_client_send_tool_result(const char *tool_id, cJSON *result)
{
    if (!tool_id) return ESP_ERR_INVALID_ARG;

    char *result_str = NULL;
    if (result) {
        result_str = cJSON_PrintUnformatted(result);
    }

    cJSON *frame = dsh_frame_build_tool_result(tool_id, "success", result_str, NULL);
    free(result_str);
    if (!frame) return ESP_ERR_NO_MEM;

    char *text = cJSON_PrintUnformatted(frame);
    cJSON_Delete(frame);
    if (!text) return ESP_ERR_NO_MEM;

    esp_err_t err = ESP_OK;
    if (s_transport && s_transport->send_text) {
        err = s_transport->send_text(text, strlen(text));
    } else {
        err = ESP_ERR_NOT_SUPPORTED;
    }
    free(text);
    return err;
}

esp_err_t dsh_client_send_user_input(const char *text)
{
    if (!text) return ESP_ERR_INVALID_ARG;

    cJSON *frame = dsh_frame_build_user_input(text);
    if (!frame) return ESP_ERR_NO_MEM;

    char *json_text = cJSON_PrintUnformatted(frame);
    cJSON_Delete(frame);
    if (!json_text) return ESP_ERR_NO_MEM;

    esp_err_t err = ESP_OK;
    if (s_transport && s_transport->send_text) {
        err = s_transport->send_text(json_text, strlen(json_text));
    } else {
        err = ESP_ERR_NOT_SUPPORTED;
    }
    free(json_text);
    return err;
}

esp_err_t dsh_client_send(const char *frame_type, cJSON *payload)
{
    if (!frame_type) return ESP_ERR_INVALID_ARG;

    cJSON *frame = dsh_frame_build_generic(frame_type, payload);
    if (!frame) return ESP_ERR_NO_MEM;

    char *text = cJSON_PrintUnformatted(frame);
    cJSON_Delete(frame);
    if (!text) return ESP_ERR_NO_MEM;

    esp_err_t err = ESP_OK;
    if (s_transport && s_transport->send_text) {
        err = s_transport->send_text(text, strlen(text));
    } else {
        err = ESP_ERR_NOT_SUPPORTED;
    }
    free(text);
    return err;
}

esp_err_t dsh_client_send_audio(const uint8_t *data, size_t len)
{
    /* W3: 走 transport->send_binary (WS Binary 帧) */
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;
    if (!s_transport || !s_transport->send_binary) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return s_transport->send_binary(data, len);
}

/* ══════════════════════════════════════════════════════════
 * 心跳控制
 * ══════════════════════════════════════════════════════════ */

esp_err_t dsh_client_start_heartbeat(uint32_t interval_ms)
{
    dsh_heartbeat_stop();
    return dsh_heartbeat_start(interval_ms, heartbeat_status_provider, NULL);
}

esp_err_t dsh_client_stop_heartbeat(void)
{
    return dsh_heartbeat_stop();
}

/* ══════════════════════════════════════════════════════════
 * 网络接口 + 状态提供者 + 传输层切换
 * ══════════════════════════════════════════════════════════ */

esp_err_t dsh_client_attach_netif(esp_netif_t *netif)
{
    /* 仅对 esp_websocket_client 传输层有效 */
    ESP_LOGI(TAG, "attach_netif: %p (WiFi transport only)", netif);
    return ESP_OK;
}

esp_err_t dsh_client_set_status_provider(dsh_client_status_cb_t cb, void *user_data)
{
    s_status_cb = cb;
    s_status_cb_data = user_data;
    ESP_LOGI(TAG, "Status provider %s", cb ? "registered" : "cleared");
    return ESP_OK;
}

esp_err_t dsh_client_set_transport(const dsh_transport_t *transport)
{
    if (s_initialized) {
        ESP_LOGE(TAG, "Cannot change transport after init");
        return ESP_ERR_INVALID_STATE;
    }
    s_transport = transport;
    ESP_LOGI(TAG, "Transport set: %s", transport ? transport->name : "default (WS)");
    return ESP_OK;
}
