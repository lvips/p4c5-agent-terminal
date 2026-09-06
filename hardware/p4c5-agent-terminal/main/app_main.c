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
#include "esp_task_wdt.h"
#include "p4c5_board.h"
#include "p4c5_pmic.h"
#include "p4c5_display.h"
#include "p4c5_audio.h"
#include "p4c5_4g.h"
#include "config.h"
#include <esp_lcd_panel_ops.h>
#include <string.h>

static const char* TAG = "app_main";

/* ── Display 测试：画 RGB 渐变 ── */
static void draw_test_pattern(void)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)p4c5_display_get_panel();
    if (!panel) {
        ESP_LOGW(TAG, "Display panel not available, skip test pattern");
        return;
    }

    const int w = P4C5_LCD_WIDTH;   /* 480 */
    const int h = P4C5_LCD_HEIGHT;  /* 800 */
    const int line_size = w * 2;    /* RGB565 = 2 bytes/pixel */

    /* 分配一行 buffer（PSRAM） */
    uint16_t* line_buf = (uint16_t*)heap_caps_malloc(line_size, MALLOC_CAP_SPIRAM);
    if (!line_buf) {
        ESP_LOGE(TAG, "Failed to allocate line buffer");
        return;
    }

    ESP_LOGI(TAG, "Drawing RGB gradient test pattern (%dx%d)...", w, h);

    /* 画水平 RGB 渐变条纹 */
    for (int y = 0; y < h; y++) {
        /* 根据 y 坐标选择颜色区域 */
        int band = (y * 5) / h;  /* 5 bands: R, G, B, White, Black */
        for (int x = 0; x < w; x++) {
            uint8_t r, g, b;
            uint8_t intensity = (x * 255) / w;

            switch (band) {
                case 0: r = intensity; g = 0;         b = 0;         break; /* Red gradient */
                case 1: r = 0;         g = intensity; b = 0;         break; /* Green gradient */
                case 2: r = 0;         g = 0;         b = intensity; break; /* Blue gradient */
                case 3: r = intensity; g = intensity; b = intensity; break; /* Gray gradient */
                default: r = 0;        g = 0;         b = 0;         break; /* Black */
            }
            /* RGB565: RRRRR GGGGGG BBBBB */
            line_buf[x] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }
        esp_lcd_panel_draw_bitmap(panel, 0, y, w, y + 1, line_buf);
    }

    free(line_buf);
    ESP_LOGI(TAG, "Test pattern drawn ✅");
}

void app_main(void)
{
    /* Watchdog: 30s timeout, panic on trigger */
    esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms = 30000,
        .trigger_panic = true,
    };
    esp_task_wdt_init(&wdt_cfg);
    esp_task_wdt_add(NULL);  /* 注册 main task */

    ESP_LOGI(TAG, "=== p4c5-agent-terminal v%s ===", P4C5_BOARD_VERSION);
    ESP_LOGI(TAG, "M7 test build: audio + display 实测 + watchdog");

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

    /* ═══════ M7 实测 ═══════ */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "════════ M7 实测开始 ════════");

    /* [M7-A] Display 测试图案 */
    ESP_LOGI(TAG, "[M7-A] Display test pattern (RGB gradient)...");
    draw_test_pattern();

    /* [M7-B] Audio 1kHz 测试音 */
    ESP_LOGI(TAG, "[M7-B] Audio 1kHz test tone (5s)...");
    p4c5_audio_set_volume(70);
    p4c5_audio_set_mute(false);
    err = p4c5_audio_test_tone(1000, 5000);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "  ✅ Audio 1kHz test tone played");
    } else {
        ESP_LOGE(TAG, "  ❌ Audio test tone failed: %s", esp_err_to_name(err));
    }

    /* [M7-C] Audio 音量渐变 */
    ESP_LOGI(TAG, "[M7-C] Audio volume ramp (0→80→0)...");
    for (int v = 0; v <= 80; v += 10) {
        p4c5_audio_set_volume(v);
        vTaskDelay(pdMS_TO_TICKS(80));
    }
    for (int v = 80; v >= 0; v -= 10) {
        p4c5_audio_set_volume(v);
        vTaskDelay(pdMS_TO_TICKS(80));
    }
    ESP_LOGI(TAG, "  ✅ Volume ramp done");

    /* [M7-D] Display 背光渐变 */
    ESP_LOGI(TAG, "[M7-D] Display backlight ramp (0→255→80)...");
    for (int b = 0; b <= 255; b += 25) {
        p4c5_display_bl_set(b);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    p4c5_display_bl_set(80);  /* 恢复到 80% */
    ESP_LOGI(TAG, "  ✅ Backlight ramp done");

    /* [M7-E] PMIC 状态 */
    ESP_LOGI(TAG, "[M7-E] PMIC status:");
    ESP_LOGI(TAG, "  battery = %u%%", p4c5_pmic_get_battery_level());
    ESP_LOGI(TAG, "  charging = %d", p4c5_pmic_is_charging());
    ESP_LOGI(TAG, "  discharging = %d", p4c5_pmic_is_discharging());
    ESP_LOGI(TAG, "  Vbat = %u mV", p4c5_pmic_get_vbat_mv());
    ESP_LOGI(TAG, "  Die temp = %d.%d°C",
             p4c5_pmic_get_die_temp_x10() / 10,
             p4c5_pmic_get_die_temp_x10() % 10);

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "════════ M7 实测完成 ════════");
    ESP_LOGI(TAG, "");

    /* TODO(M4): 启动 LVGL UI 任务 */
    /* TODO(M4): 启动 WebSocket 协议任务（dsh_client） */
    /* TODO(M4): 启动音频流任务 */

    /* 主循环 */
    while (1) {
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "heartbeat: bat=%u%%, charging=%d, vbat=%umV, temp=%d°C",
                 p4c5_pmic_get_battery_level(),
                 p4c5_pmic_is_charging(),
                 p4c5_pmic_get_vbat_mv(),
                 p4c5_pmic_get_die_temp_x10() / 10);
    }
}
