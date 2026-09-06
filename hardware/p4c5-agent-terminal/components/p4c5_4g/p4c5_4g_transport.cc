/**
 * @file p4c5_4g_transport.cc
 * @brief ML307 WebSocket 传输层适配器
 *
 * 将 esp-ml307 的 WebSocket 类适配为 dsh_transport_t 接口，
 * 使 dsh_client 可以通过 4G 网络连接 DSH 服务端。
 *
 * 数据流：
 *   dsh_client → dsh_transport_t.send_text() → WebSocket.Send()
 *   WebSocket.OnData() → dsh_transport_json_cb_t()
 *   WebSocket.OnConnected/OnDisconnected → dsh_transport_state_cb_t()
 */

#include "dsh_client_transport.h"
#include "p4c5_4g.h"

#include <esp_log.h>
#include "network_interface.h"
#include "web_socket.h"

#include <cstring>
#include <string>
#include <memory>

static const char *TAG = "4g_transport";

/* ── 内部状态 ── */
static std::unique_ptr<WebSocket> s_ws;
static std::string s_uri;
static dsh_transport_state_cb_t s_state_cb = nullptr;
static void *s_state_cb_data = nullptr;
static dsh_transport_json_cb_t s_json_cb = nullptr;
static void *s_json_cb_data = nullptr;

/* ── 传输层实现 ── */

static esp_err_t ml307_init(const char *uri, uint32_t buffer_size)
{
    if (!uri) return ESP_ERR_INVALID_ARG;
    s_uri = uri;
    ESP_LOGI(TAG, "ML307 transport init: uri=%s", uri);
    return ESP_OK;
}

static void ml307_deinit(void)
{
    s_ws.reset();
    s_uri.clear();
    ESP_LOGI(TAG, "ML307 transport deinit");
}

static esp_err_t ml307_start(void)
{
    NetworkInterface *network = p4c5_4g_internal::get_network();
    if (!network) {
        ESP_LOGE(TAG, "4G network not available");
        return ESP_ERR_INVALID_STATE;
    }

    if (!p4c5_4g_is_network_ready()) {
        ESP_LOGW(TAG, "4G network not ready yet");
        return ESP_ERR_INVALID_STATE;
    }

    /* 创建 WebSocket（连接 ID = 1） */
    s_ws = network->CreateWebSocket(1);
    if (!s_ws) {
        ESP_LOGE(TAG, "Failed to create ML307 WebSocket");
        return ESP_FAIL;
    }

    /* 注册回调 */
    s_ws->OnConnected([]() {
        ESP_LOGI(TAG, "ML307 WebSocket connected");
        if (s_state_cb) s_state_cb(true, s_state_cb_data);
    });

    s_ws->OnDisconnected([]() {
        ESP_LOGW(TAG, "ML307 WebSocket disconnected");
        if (s_state_cb) s_state_cb(false, s_state_cb_data);
    });

    s_ws->OnData([](const char *data, size_t len, bool binary) {
        if (binary) {
            ESP_LOGD(TAG, "RX binary: %zu bytes", len);
            return;
        }

        /* TEXT 帧：解析 JSON + 提取 type */
        std::string json_text(data, len);

        /* 使用 cJSON 解析 */
        cJSON *json = cJSON_Parse(json_text.c_str());
        if (!json) {
            ESP_LOGW(TAG, "JSON parse failed: %.128s", json_text.c_str());
            return;
        }

        cJSON *type_item = cJSON_GetObjectItemCaseSensitive(json, "type");
        const char *frame_type = cJSON_IsString(type_item) ? type_item->valuestring : "unknown";

        if (s_json_cb) {
            s_json_cb(json_text.c_str(), frame_type, json, s_json_cb_data);
        }

        cJSON_Delete(json);
    });

    s_ws->OnError([](int error) {
        ESP_LOGE(TAG, "ML307 WebSocket error: %d", error);
    });

    /* 连接到 DSH 服务端 */
    ESP_LOGI(TAG, "Connecting to %s...", s_uri.c_str());
    if (!s_ws->Connect(s_uri.c_str())) {
        ESP_LOGE(TAG, "WebSocket Connect failed");
        s_ws.reset();
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "ML307 WebSocket connecting...");
    return ESP_OK;
}

static esp_err_t ml307_stop(void)
{
    if (s_ws) {
        s_ws->Close();
        s_ws.reset();
    }
    ESP_LOGI(TAG, "ML307 transport stopped");
    return ESP_OK;
}

static esp_err_t ml307_send_text(const char *text, size_t len)
{
    if (!s_ws || !s_ws->IsConnected()) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_ws->Send(std::string(text, len))) {
        ESP_LOGE(TAG, "Send failed");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "TX TEXT (%zu): %.128s...", len, text);
    return ESP_OK;
}

static bool ml307_is_connected(void)
{
    return s_ws && s_ws->IsConnected();
}

static void ml307_set_state_cb(dsh_transport_state_cb_t cb, void *user_data)
{
    s_state_cb = cb;
    s_state_cb_data = user_data;
}

static void ml307_set_json_cb(dsh_transport_json_cb_t cb, void *user_data)
{
    s_json_cb = cb;
    s_json_cb_data = user_data;
}

/* ── 传输层 vtable ── */
static const dsh_transport_t s_ml307_transport = {
    .name         = "ml307_websocket",
    .init         = ml307_init,
    .deinit       = ml307_deinit,
    .start        = ml307_start,
    .stop         = ml307_stop,
    .send_text    = ml307_send_text,
    .is_connected = ml307_is_connected,
    .set_state_cb = ml307_set_state_cb,
    .set_json_cb  = ml307_set_json_cb,
};

/* ══════════════════════════════════════════════════════════
 * 公开 C API
 * ══════════════════════════════════════════════════════════ */

extern "C" {

const dsh_transport_t *p4c5_4g_get_transport(void)
{
    if (!p4c5_4g_is_modem_detected()) {
        ESP_LOGW(TAG, "Modem not detected, transport unavailable");
        return nullptr;
    }
    return &s_ml307_transport;
}

}  /* extern "C" */
