/**
 * @file app_main.c
 * @brief p4c5-agent-terminal 固件入口 (v0.1.0)
 *
 * 初始化顺序（按硬件依赖）：
 *   I2C bus (p4c5_board) → PMIC (p4c5_pmic) → Display (p4c5_display) → Audio (p4c5_audio) → 4G (p4c5_4g)
 *
 * ⚠️ 不使用 p4c5_board_init() 中间层（解决 p4c5_* 静态库链接符号问题）。
 *    各子模块 init 由 main.c 直接调用。
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "p4c5_board.h"
#include "p4c5_pmic.h"
#include "p4c5_display.h"
#include "p4c5_audio.h"
#include "p4c5_4g.h"
#include "config.h"

static const char* TAG = "app_main";

void app_main(void)
{
    ESP_LOGI(TAG, "=== p4c5-agent-terminal v%s ===", P4C5_BOARD_VERSION);
    ESP_LOGW(TAG, "⚠️  Pre-M0 build: 仅验证 boot 链路，所有子系统将逐步启用");

    /* 0. 板级共享资源 (I2C bus) */
    ESP_LOGI(TAG, "[0/4] Board init (I2C bus)...");
    esp_err_t err = p4c5_board_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Board init failed: %s", esp_err_to_name(err));
        return;
    }

    /* 1. PMIC (AXP2101) - 必须先初始化，display/audio/4g 都依赖电源轨 */
    ESP_LOGI(TAG, "[1/4] PMIC init (AXP2101, 14 寄存器 + 0x64=0x2B 修正)...");
    err = p4c5_pmic_init(p4c5_board_get_i2c_bus());
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PMIC init failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "  PMIC battery: %u%%, charging=%d, vbat=%u mV",
             p4c5_pmic_get_battery_level(),
             p4c5_pmic_is_charging(),
             p4c5_pmic_get_vbat_mv());

    /* 2. Display (ST7102 MIPI-DSI + ST7123 触摸) */
    ESP_LOGI(TAG, "[2/4] Display init (ST7102 480x800)...");
    err = p4c5_display_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Display init failed: %s", esp_err_to_name(err));
        return;
    }
    p4c5_display_bl_set(80);  /* 默认背光 80% */
    ESP_LOGI(TAG, "  Display backlight: %u%%", p4c5_display_bl_get());

    /* 3. Audio (ES8311 DAC + ES7210 ADC, 24kHz) */
    ESP_LOGI(TAG, "[3/4] Audio init (ES8311+ES7210, 24kHz)...");
    p4c5_audio_set_i2c_bus(p4c5_board_get_i2c_bus());
    err = p4c5_audio_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Audio init failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "  Audio init OK");

    /* 4. 4G (ML307C, esp-ml307 协议栈) */
    ESP_LOGI(TAG, "[4/4] 4G init (ML307C, baud=921600)...");
    err = p4c5_4g_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "4G init failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "  4G init OK");

    ESP_LOGI(TAG, "=========================================");
    ESP_LOGI(TAG, "  All subsystems initialized successfully");
    ESP_LOGI(TAG, "  v%s ready", P4C5_BOARD_VERSION);
    ESP_LOGI(TAG, "=========================================");

    /* TODO(M4): 启动 LVGL UI 任务 */
    /* TODO(M4): 启动 WebSocket 协议任务（dsh_client） */
    /* TODO(M4): 启动音频流任务 */

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "heartbeat: bat=%u%%, charging=%d",
                 p4c5_pmic_get_battery_level(),
                 p4c5_pmic_is_charging());
    }
}
