/**
 * @file dsh_client_transport_ws.c
 * @brief esp_websocket_client 传输层适配器
 *
 * 将现有的 dsh_client_ws.h 内部接口适配为 dsh_transport_t 接口。
 * 用于 WiFi / 以太网场景。
 */

#include "dsh_client_transport.h"
#include "dsh_client_ws.h"
#include <string.h>

static esp_err_t ws_init(const char *uri, uint32_t buffer_size)
{
    return dsh_ws_init(uri, buffer_size);
}

static void ws_deinit(void)
{
    dsh_ws_deinit();
}

static esp_err_t ws_start(void)
{
    return dsh_ws_start();
}

static esp_err_t ws_stop(void)
{
    return dsh_ws_stop();
}

static esp_err_t ws_send_text(const char *text, size_t len)
{
    (void)len;
    return dsh_ws_send_text(text);
}

static esp_err_t ws_send_binary(const uint8_t *data, size_t len)
{
    return dsh_ws_send_binary(data, len);
}

static bool ws_is_connected(void)
{
    return dsh_ws_is_connected();
}

static void ws_set_state_cb(dsh_transport_state_cb_t cb, void *user_data)
{
    /* dsh_ws_set_state_callback 的签名与 dsh_transport_state_cb_t 一致 */
    dsh_ws_set_state_callback((dsh_ws_state_cb_t)cb, user_data);
}

static void ws_set_json_cb(dsh_transport_json_cb_t cb, void *user_data)
{
    /* dsh_ws_set_json_callback 的签名与 dsh_transport_json_cb_t 一致 */
    dsh_ws_set_json_callback((dsh_ws_json_cb_t)cb, user_data);
}

const dsh_transport_t dsh_transport_ws = {
    .name         = "esp_websocket_client",
    .init         = ws_init,
    .deinit       = ws_deinit,
    .start        = ws_start,
    .stop         = ws_stop,
    .send_text    = ws_send_text,
    .send_binary  = ws_send_binary,
    .is_connected = ws_is_connected,
    .set_state_cb = ws_set_state_cb,
    .set_json_cb  = ws_set_json_cb,
};

const dsh_transport_t *dsh_transport_get_ws(void)
{
    return &dsh_transport_ws;
}
