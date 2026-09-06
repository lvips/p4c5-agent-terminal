/**
 * @file app_main.c
 * @brief p4c5-agent-terminal 固件入口
 *
 * 最小骨架 — 仅调用 p4c5_board_init() 统一初始化。
 * 后续迭代中各子系统（音频、显示、4G、协议）将在 board init 后依次启动。
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "p4c5_board.h"

static const char* TAG = "app_main";

void app_main(void)
{
    ESP_LOGI(TAG, "=== p4c5-agent-terminal v0.1.0 ===");
    ESP_LOGI(TAG, "Board version: %s", P4C5_BOARD_VERSION);

    /* 板级统一初始化（PMIC → Display → Audio → 4G） */
    esp_err_t err = p4c5_board_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Board init failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Board init complete. Entering main loop.");

    /* TODO: 启动 LVGL UI 任务 */
    /* TODO: 启动 4G 网络连接任务 */
    /* TODO: 启动 WebSocket 协议任务（dsh_client） */
    /* TODO: 启动音频流任务 */

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        // TODO: 心跳 / 状态机驱动
    }
}
