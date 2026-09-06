/**
 * @file dsh_client.c
 * @brief DSH 客户端主控制逻辑
 *
 * 协调 WebSocket 传输层、帧构造/解析、心跳任务。
 * 对外提供 13 个 API。
 */

#include "dsh_client.h"
#include "dsh_client_ws.h"
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

/* 回调 */
static dsh_client_event_cb_t  s_frame_cb     = NULL;
static void                  *s_frame_cb_data = NULL;
static dsh_client_state_cb_t  s_state_cb     = NULL;
static void                  *s_state_cb_data = NULL;
static dsh_client_status_cb_t s_status_cb    = NULL;
static void                  *s_status_cb_data = NULL;

/* ══════════════════════════════════════════════════════════
 * 内部回调桥接
 * ══════════════════════════════════════════════════════════ */

/**
 * WebSocket JSON 帧回调 — 转发给应用层
 */
static void on_ws_json(const char *json_text, const char *frame_type,
                        cJSON *json, void *user_data)
{
    ESP_LOGD(TAG, "Frame RX: type=%s", frame_type ? frame_type : "null");

    if (s_frame_cb) {
        s_frame_cb(frame_type, json, s_frame_cb_data);
    }
}

/**
 * WebSocket 状态回调 — 管理连接状态 + 自动发送 HELLO + 心跳
 */
static void on_ws_state(bool connected, void *user_data)
{
    s_connected = connected;

    if (connected) {
        ESP_LOGI(TAG, "Connected → sending HELLO");

        /* 发送 client/hello */
        dsh_client_send_hello();

        /* 通知应用层 */
        if (s_state_cb) {
            s_state_cb(DSH_EVENT_CONNECTED, s_state_cb_data);
        }
    } else {
        ESP_LOGW(TAG, "Disconnected");

        /* 停止心跳（重连成功后会自动重启） */
        dsh_heartbeat_stop();

        if (s_state_cb) {
            s_state_cb(DSH_EVENT_DISCONNECTED, s_state_cb_data);
        }
    }
}

/**
 * 心跳状态查询回调桥接
 */
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

    /* 初始化 WebSocket 传输层 */
    esp_err_t err = dsh_ws_init(s_cfg.url, s_cfg.ws_buffer_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WS init failed: %s", esp_err_to_name(err));
        return err;
    }

    /* 注册内部回调 */
    dsh_ws_set_json_callback(on_ws_json, NULL);
    dsh_ws_set_state_callback(on_ws_state, NULL);

    s_initialized = true;
    ESP_LOGI(TAG, "DSH client initialized: url=%s, device=%s", s_cfg.url, s_cfg.device_id);
    return ESP_OK;
}

esp_err_t dsh_client_connect(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_connected) {
        ESP_LOGW(TAG, "Already connected");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Connecting to %s...", s_cfg.url);

    esp_err_t err = dsh_ws_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WS start failed: %s", esp_err_to_name(err));
        return err;
    }

    /* 启动心跳（默认 30s） */
    dsh_client_start_heartbeat(30000);

    return ESP_OK;
}

esp_err_t dsh_client_disconnect(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    dsh_heartbeat_stop();
    dsh_ws_stop();
    s_connected = false;

    ESP_LOGI(TAG, "Disconnected");
    return ESP_OK;
}

void dsh_client_deinit(void)
{
    if (!s_initialized) return;

    dsh_client_disconnect();
    dsh_ws_deinit();

    free(s_cfg.url);
    free(s_cfg.device_id);
    free(s_cfg.auth_token);
    memset(&s_cfg, 0, sizeof(s_cfg));

    s_frame_cb = NULL;
    s_state_cb = NULL;
    s_status_cb = NULL;
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
        s_cfg.device_id,
        DSH_CLIENT_VERSION,
        NULL,  /* 使用默认能力集 */
        s_cfg.auth_token
    );
    if (!hello) return ESP_ERR_NO_MEM;

    esp_err_t err = dsh_ws_send_json(hello);
    cJSON_Delete(hello);
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

    esp_err_t err = dsh_ws_send_json(hb);
    cJSON_Delete(hb);
    return err;
}

esp_err_t dsh_client_send_tool_result(const char *tool_id, cJSON *result)
{
    if (!tool_id) return ESP_ERR_INVALID_ARG;

    const char *result_str = NULL;
    char *result_json = NULL;

    /* 如果 result 是 cJSON 对象，序列化为字符串 */
    if (result) {
        result_json = cJSON_PrintUnformatted(result);
        result_str = result_json;
    }

    cJSON *frame = dsh_frame_build_tool_result(tool_id, "success", result_str, NULL);
    free(result_json);

    if (!frame) return ESP_ERR_NO_MEM;

    esp_err_t err = dsh_ws_send_json(frame);
    cJSON_Delete(frame);
    return err;
}

esp_err_t dsh_client_send_user_input(const char *text)
{
    if (!text) return ESP_ERR_INVALID_ARG;

    cJSON *frame = dsh_frame_build_user_input(text);
    if (!frame) return ESP_ERR_NO_MEM;

    esp_err_t err = dsh_ws_send_json(frame);
    cJSON_Delete(frame);
    return err;
}

esp_err_t dsh_client_send(const char *frame_type, cJSON *payload)
{
    if (!frame_type) return ESP_ERR_INVALID_ARG;

    cJSON *frame = dsh_frame_build_generic(frame_type, payload);
    if (!frame) return ESP_ERR_NO_MEM;

    esp_err_t err = dsh_ws_send_json(frame);
    cJSON_Delete(frame);
    return err;
}

/* ══════════════════════════════════════════════════════════
 * 心跳控制
 * ══════════════════════════════════════════════════════════ */

esp_err_t dsh_client_start_heartbeat(uint32_t interval_ms)
{
    /* 先停止旧的（如果有） */
    dsh_heartbeat_stop();
    return dsh_heartbeat_start(interval_ms, heartbeat_status_provider, NULL);
}

esp_err_t dsh_client_stop_heartbeat(void)
{
    return dsh_heartbeat_stop();
}

/* ══════════════════════════════════════════════════════════
 * 网络接口
 * ══════════════════════════════════════════════════════════ */

esp_err_t dsh_client_attach_netif(esp_netif_t *netif)
{
    return dsh_ws_attach_netif(netif);
}

/* ══════════════════════════════════════════════════════════
 * 状态提供者
 * ══════════════════════════════════════════════════════════ */

esp_err_t dsh_client_set_status_provider(dsh_client_status_cb_t cb, void *user_data)
{
    s_status_cb = cb;
    s_status_cb_data = user_data;
    ESP_LOGI(TAG, "Status provider %s", cb ? "registered" : "cleared");
    return ESP_OK;
}
