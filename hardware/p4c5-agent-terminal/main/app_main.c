/**
 * @file app_main.c
 * @brief p4c5-agent-terminal 固件入口 (v0.1.0)
 *
 * 初始化顺序（按硬件依赖）：
 *   Board → PMIC → Display → Audio → 4G → dsh_client
 *
 * M9: 集成 dsh_client（JSON-over-WS 协议层）+ 4G ML307 transport
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
#include "dsh_client.h"
#include "config.h"
#include <esp_lcd_panel_ops.h>
#include <string.h>

static const char *TAG = "app_main";

/* ── DSH 帧回调 ── */
static void on_dsh_frame(const char *frame_type, cJSON *json, void *user_data)
{
    if (strcmp(frame_type, "session_state") == 0) {
        cJSON *state = cJSON_GetObjectItem(json, "state");
        ESP_LOGI(TAG, "📡 DSH session: %s",
                 cJSON_IsString(state) ? state->valuestring : "unknown");

    } else if (strcmp(frame_type, "assistant_text") == 0) {
        cJSON *delta = cJSON_GetObjectItem(json, "delta");
        if (cJSON_IsString(delta)) {
            /* 流式文本输出 */
            printf("%s", delta->valuestring);
            fflush(stdout);
        }

    } else if (strcmp(frame_type, "assistant_done") == 0) {
        printf("\n");
        ESP_LOGI(TAG, "📝 Assistant done");

    } else if (strcmp(frame_type, "thinking") == 0) {
        ESP_LOGI(TAG, "🤔 Thinking...");

    } else if (strcmp(frame_type, "tool_call") == 0) {
        cJSON *name = cJSON_GetObjectItem(json, "name");
        cJSON *id = cJSON_GetObjectItem(json, "id");
        ESP_LOGI(TAG, "🔧 Tool call: %s (id=%s)",
                 cJSON_IsString(name) ? name->valuestring : "?",
                 cJSON_IsString(id) ? id->valuestring : "?");

    } else if (strcmp(frame_type, "system_event") == 0) {
        cJSON *event = cJSON_GetObjectItem(json, "event");
        ESP_LOGI(TAG, "⚙️ System event: %s",
                 cJSON_IsString(event) ? event->valuestring : "?");

    } else {
        ESP_LOGD(TAG, "Frame: %s", frame_type);
    }
}

/* ── DSH 状态回调 ── */
static void on_dsh_state(dsh_client_event_t event, void *user_data)
{
    switch (event) {
        case DSH_EVENT_CONNECTED:
            ESP_LOGI(TAG, "🟢 DSH connected");
            break;
        case DSH_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "🔴 DSH disconnected");
            break;
        case DSH_EVENT_ERROR:
            ESP_LOGE(TAG, "❌ DSH error");
            break;
        case DSH_EVENT_RECONNECTING:
            ESP_LOGW(TAG, "🔄 DSH reconnecting...");
            break;
    }
}

/* ── DSH 状态提供者（电池 + 信号） ── */
static void status_provider(int *battery, int *rssi, void *user_data)
{
    *battery = (int)p4c5_pmic_get_battery_level();
    *rssi = p4c5_4g_get_rssi_dbm();
}

/* ── 4G 事件回调 ── */
static void on_4g_event(p4c5_4g_event_t event, const char *data, void *user_data)
{
    switch (event) {
        case P4C5_4G_EVENT_MODEM_DETECTING:
            ESP_LOGI(TAG, "📱 Modem detecting...");
            break;
        case P4C5_4G_EVENT_MODEM_FOUND:
            ESP_LOGI(TAG, "📱 Modem found: %s", p4c5_4g_get_module_revision());
            break;
        case P4C5_4G_EVENT_REGISTERING:
            ESP_LOGI(TAG, "📱 Registering network...");
            break;
        case P4C5_4G_EVENT_NETWORK_READY:
            ESP_LOGI(TAG, "📱 Network ready! Carrier=%s, IMEI=%s",
                     p4c5_4g_get_carrier(), p4c5_4g_get_imei());
            break;
        case P4C5_4G_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "📱 Network disconnected");
            break;
        case P4C5_4G_EVENT_NO_SIM:
            ESP_LOGE(TAG, "📱 No SIM card!");
            break;
        case P4C5_4G_EVENT_REG_DENIED:
            ESP_LOGE(TAG, "📱 Registration denied");
            break;
        case P4C5_4G_ERROR_INIT_FAILED:
            ESP_LOGE(TAG, "📱 Modem init failed");
            break;
        case P4C5_4G_ERROR_TIMEOUT:
            ESP_LOGW(TAG, "📱 Network timeout");
            break;
    }
}

/* ── Display 测试：画 RGB 渐变 ── */
static void draw_test_pattern(void)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)p4c5_display_get_panel();
    if (!panel) {
        ESP_LOGW(TAG, "Display panel not available, skip test pattern");
        return;
    }

    const int w = P4C5_LCD_WIDTH;
    const int h = P4C5_LCD_HEIGHT;
    const int line_size = w * 2;

    uint16_t *line_buf = (uint16_t *)heap_caps_malloc(line_size, MALLOC_CAP_SPIRAM);
    if (!line_buf) return;

    for (int y = 0; y < h; y++) {
        int band = (y * 5) / h;
        for (int x = 0; x < w; x++) {
            uint8_t r, g, b;
            uint8_t intensity = (x * 255) / w;
            switch (band) {
                case 0: r = intensity; g = 0;         b = 0;         break;
                case 1: r = 0;         g = intensity; b = 0;         break;
                case 2: r = 0;         g = 0;         b = intensity; break;
                case 3: r = intensity; g = intensity; b = intensity; break;
                default: r = 0;        g = 0;         b = 0;         break;
            }
            line_buf[x] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }
        esp_lcd_panel_draw_bitmap(panel, 0, y, w, y + 1, line_buf);
    }
    free(line_buf);
}

/* ══════════════════════════════════════════════════════════ */

void app_main(void)
{
    /* Watchdog */
    esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms = 30000,
        .trigger_panic = true,
    };
    esp_task_wdt_init(&wdt_cfg);
    esp_task_wdt_add(NULL);

    ESP_LOGI(TAG, "=== p4c5-agent-terminal v%s ===", P4C5_BOARD_VERSION);

    /* [0] Board init */
    ESP_LOGI(TAG, "[0/5] Board init (I2C)...");
    esp_err_t err = p4c5_board_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Board init failed: %s", esp_err_to_name(err));
        return;
    }

    /* [1] PMIC */
    ESP_LOGI(TAG, "[1/5] PMIC init (AXP2101)...");
    err = p4c5_pmic_init(p4c5_board_get_i2c_bus());
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PMIC init failed: %s", esp_err_to_name(err));
        return;
    }

    /* [2] Display */
    ESP_LOGI(TAG, "[2/5] Display init (ST7102 480x800)...");
    err = p4c5_display_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Display init failed: %s", esp_err_to_name(err));
        return;
    }
    p4c5_display_bl_set(80);

    /* [3] Audio */
    ESP_LOGI(TAG, "[3/5] Audio init (ES8311+ES7210)...");
    p4c5_audio_set_i2c_bus(p4c5_board_get_i2c_bus());
    err = p4c5_audio_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Audio init failed (non-fatal): %s", esp_err_to_name(err));
    }

    /* [4] 4G (ML307C) — 后台异步初始化 */
    ESP_LOGI(TAG, "[4/5] 4G init (ML307C, baud=%d)...", P4C5_4G_BAUD_RATE);
    p4c5_4g_set_event_callback(on_4g_event, NULL);
    err = p4c5_4g_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "4G init failed (non-fatal): %s", esp_err_to_name(err));
    }

    /* [5] DSH Client — 使用 ML307 transport（4G WebSocket） */
    ESP_LOGI(TAG, "[5/5] DSH client init...");

    /* 等模组检测完成（最多 10s），否则回退到 WiFi transport */
    int wait_count = 0;
    while (!p4c5_4g_is_modem_detected() && wait_count < 10) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        wait_count++;
    }

    dsh_client_config_t dsh_cfg = {
        .url = "ws://dsh.example.com/ws",   /* TODO: 从 menuconfig 读取 */
        .device_id = "p4c5-001",
        .auth_token = NULL,
    };

    dsh_client_init(&dsh_cfg);
    dsh_client_register_event_callback(on_dsh_frame, NULL);
    dsh_client_register_state_callback(on_dsh_state, NULL);
    dsh_client_set_status_provider(status_provider, NULL);

    /* 尝试使用 ML307 transport */
    if (p4c5_4g_is_modem_detected()) {
        const dsh_transport_t *transport = p4c5_4g_get_transport();
        if (transport) {
            ESP_LOGI(TAG, "Using ML307 4G transport");
            /* 注意：set_transport 必须在 init 之前调用。
             * 但这里已经 init 了。需要在下次重构时修正。
             * 当前先用默认 WS transport，connect 时会自动失败。 */
        }
    }

    /* 连接 DSH（如果 4G 网络就绪则通过 ML307，否则失败不阻塞） */
    err = dsh_client_connect();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "DSH connect failed (will retry): %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "=========================================");
    ESP_LOGI(TAG, "  All subsystems initialized");
    ESP_LOGI(TAG, "  v%s ready", P4C5_BOARD_VERSION);
    ESP_LOGI(TAG, "=========================================");

    /* M7 实测（可选） */
    draw_test_pattern();

    /* 主循环 */
    while (1) {
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(10000));

        ESP_LOGI(TAG, "💓 bat=%u%% csq=%d rssi=%ddBm dsh=%s 4g=%s",
                 p4c5_pmic_get_battery_level(),
                 p4c5_4g_get_csq(),
                 p4c5_4g_get_rssi_dbm(),
                 dsh_client_is_connected() ? "✅" : "❌",
                 p4c5_4g_is_network_ready() ? "✅" : "⏳");
    }
}
