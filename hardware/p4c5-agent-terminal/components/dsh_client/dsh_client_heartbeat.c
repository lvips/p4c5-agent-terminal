/**
 * @file dsh_client_heartbeat.c
 * @brief 心跳任务实现
 *
 * 每 interval_ms 发送 client/heartbeat JSON 帧，
 * 携带时间戳 + 电池百分比 + 信号强度。
 */

#include "dsh_client_heartbeat.h"
#include "dsh_client_frames.h"
#include "dsh_client_ws.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>

static const char *TAG = "dsh_hb";

static TaskHandle_t          s_task        = NULL;
static dsh_status_provider_t s_provider    = NULL;
static void                 *s_provider_ud = NULL;
static uint32_t              s_interval_ms = 30000;

/* ── 心跳任务 ── */
static void heartbeat_task(void *arg)
{
    ESP_LOGI(TAG, "Heartbeat task started: interval=%lu ms", (unsigned long)s_interval_ms);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(s_interval_ms));

        /* 查询电池 + 信号 */
        int battery = -1;
        int rssi = 0;
        if (s_provider) {
            s_provider(&battery, &rssi, s_provider_ud);
        }

        /* 构造心跳帧 */
        cJSON *hb = dsh_frame_build_heartbeat(battery, rssi);
        if (!hb) {
            ESP_LOGE(TAG, "Failed to build heartbeat frame");
            continue;
        }

        /* 发送（如果未连接，dsh_ws_send_json 会返回 ERR） */
        esp_err_t err = dsh_ws_send_json(hb);
        cJSON_Delete(hb);

        if (err == ESP_OK) {
            ESP_LOGD(TAG, "Heartbeat sent: battery=%d%%, rssi=%d dBm", battery, rssi);
        } else {
            ESP_LOGW(TAG, "Heartbeat send failed (not connected?)");
        }
    }
}

esp_err_t dsh_heartbeat_start(uint32_t interval_ms,
                               dsh_status_provider_t provider,
                               void *user_data)
{
    if (s_task) {
        ESP_LOGW(TAG, "Already running");
        return ESP_OK;
    }

    s_interval_ms = (interval_ms > 0) ? interval_ms : 30000;
    s_provider = provider;
    s_provider_ud = user_data;

    BaseType_t ret = xTaskCreate(
        heartbeat_task,
        "dsh_hb",
        3072,           /* stack size — cJSON 需要一些栈空间 */
        NULL,
        4,              /* priority: 略低于主任务 */
        &s_task
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task");
        s_task = NULL;
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t dsh_heartbeat_stop(void)
{
    if (s_task) {
        vTaskDelete(s_task);
        s_task = NULL;
        ESP_LOGI(TAG, "Heartbeat task stopped");
    }
    return ESP_OK;
}

int dsh_heartbeat_is_running(void)
{
    return (s_task != NULL) ? 1 : 0;
}
