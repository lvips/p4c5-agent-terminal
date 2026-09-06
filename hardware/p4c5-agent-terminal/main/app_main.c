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
#include "nvs_flash.h"
#include "p4c5_board.h"
#include "p4c5_pmic.h"
#include "p4c5_display.h"
#include "p4c5_audio.h"
// #include "p4c5_4g.h"  // W1: 4G 搁置 (需电池), 改用 WiFi
#include "p4c5_ui.h"
#include "dsh_client.h"
#include "wifi_manager.h"
#include "config.h"
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

/* ─ DSH 状态提供者（电池 + WiFi RSSI） ── */
static void status_provider(int *battery, int *rssi, void *user_data)
{
    *battery = (int)p4c5_pmic_get_battery_level();
    // *rssi = p4c5_4g_get_rssi_dbm();  // W1: 4G 搁置
    *rssi = -99;  // TODO: 从 wifi_manager 获取 RSSI
}

/* ── 4G 事件回调 (W1: 4G 搁置，暂时注释) ── */
/*
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
*/

/* ══════════════════════════════════════════════════════════ */

void app_main(void)
{
    /* ── Watchdog ──────────────────────────────────────────────
     * 系统在 app_main 之前已经初始化了 TWDT（5s 超时，panic 模式）。
     * 尝试 reconfigure 为 30s；若失败（旧版 IDF 不支持）则继续
     * 使用系统默认配置，靠喂狗维持。
     * ─────────────────────────────────────────────────────────── */
    esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms = 30000,
        .trigger_panic = true,
        .idle_core_mask = 0,   /* 不监控 idle task */
    };
    esp_err_t wdt_err = esp_task_wdt_reconfigure(&wdt_cfg);
    if (wdt_err != ESP_OK) {
        ESP_LOGW(TAG, "TWDT reconfigure failed (%s), will feed existing WDT",
                 esp_err_to_name(wdt_err));
    } else {
        ESP_LOGI(TAG, "TWDT reconfigured to 30s");
    }
    /* 确保当前 main task 已注册到 WDT */
    esp_task_wdt_add(NULL);

    ESP_LOGI(TAG, "=== p4c5-agent-terminal v%s ===", P4C5_BOARD_VERSION);

    /* [0] Board init */
    ESP_LOGI(TAG, "[0/5] Board init (I2C)...");
    esp_err_t err = p4c5_board_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Board init failed: %s", esp_err_to_name(err));
        return;
    }
    esp_task_wdt_reset();   /* 喂狗 */

    /* [1] PMIC */
    ESP_LOGI(TAG, "[1/5] PMIC init (AXP2101)...");
    err = p4c5_pmic_init(p4c5_board_get_i2c_bus());
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PMIC init failed: %s", esp_err_to_name(err));
        return;
    }
    p4c5_pmic_print_adc();  /* T15 自测电源 */
    esp_task_wdt_reset();

    /* [2] Display */
    ESP_LOGI(TAG, "[2/5] Display init (ST7102 480x800)...");
    err = p4c5_display_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Display init failed: %s", esp_err_to_name(err));
        return;
    }
    p4c5_display_bl_set(80);
    esp_task_wdt_reset();

    /* [3] Audio */
    ESP_LOGI(TAG, "[3/5] Audio init (ES8311+ES7210)...");
    p4c5_audio_set_i2c_bus(p4c5_board_get_i2c_bus());
    err = p4c5_audio_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Audio init failed (non-fatal): %s", esp_err_to_name(err));
    }
    esp_task_wdt_reset();

    /* [4] WiFi (ESP32-C5 via SDIO) — W1: 4G 搁置，改用 WiFi */
    ESP_LOGI(TAG, "[4/6] WiFi init (esp_hosted + wifi_manager)...");

    /* NVS 初始化 (WiFi 配置持久化) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* WiFi manager 初始化 */
    ESP_ERROR_CHECK(wifi_manager_init());

    /* 启动 WiFi (硬编码 SSID 测试) */
    wifi_manager_config_t wifi_cfg = {
        .sta_ssid = "ZTE-SONG-2.4G",              // 2.4GHz (C5 兼容性更好)
        .sta_password = "51UPSONG99",          // 用户家 WiFi 密码
        .ap_ssid_prefix = "p4c5-agent",
        .ap_ssid = NULL,
        .ap_password = NULL,
        .ap_behavior = "keep",  // W1 fix: "fallback" 无效, 有效值: "" / "keep" / "close_on_sta"
        .ap_channel = 1,
        .ap_max_conn = 4,
        .max_retry = 5,
    };
    ESP_ERROR_CHECK(wifi_manager_start(&wifi_cfg));

    /* 等待 STA 连接 (最多 20s — 必须 < TWDT 30s timeout) */
    ESP_LOGI(TAG, "Waiting for WiFi STA connection (max 20s)...");
    esp_err_t wifi_ret = wifi_manager_wait_connected(20000);
    if (wifi_ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi STA connected ✅");
    } else {
        ESP_LOGW(TAG, "WiFi STA connection timeout (fallback to AP mode)");
    }
    esp_task_wdt_reset();  /* TWDT reset (WiFi wait 不能超过 TWDT) */

    /* [5] DSH Client — WiFi transport */
    ESP_LOGI(TAG, "[5/6] DSH client init...");

    dsh_client_config_t dsh_cfg = {
        .url = CONFIG_P4C5_DSH_WEBSOCKET_URL,
        .device_id = CONFIG_P4C5_DSH_DEVICE_ID,
        .auth_token = (strlen(CONFIG_P4C5_DSH_AUTH_TOKEN) > 0)
                      ? CONFIG_P4C5_DSH_AUTH_TOKEN : NULL,
    };

    dsh_client_init(&dsh_cfg);
    dsh_client_register_event_callback(on_dsh_frame, NULL);
    dsh_client_register_state_callback(on_dsh_state, NULL);
    dsh_client_set_status_provider(status_provider, NULL);

    /* 连接 DSH via WiFi */
    err = dsh_client_connect();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "DSH connect via WiFi failed (will retry): %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "=========================================");
    ESP_LOGI(TAG, "  All subsystems initialized");
    ESP_LOGI(TAG, "  v%s ready", P4C5_BOARD_VERSION);
    ESP_LOGI(TAG, "=========================================");

    /* M7 → T14: 测试图 → LVGL 真 UI */
    err = p4c5_ui_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "LVGL UI init failed (non-fatal): %s", esp_err_to_name(err));
    }

    /* 主循环 */
    while (1) {
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(10000));

        /* W1 fix: 用 wifi_manager_get_status() 真实检测 WiFi 状态 */
        wifi_manager_status_t wifi_st = {0};
        wifi_manager_get_status(&wifi_st);
        ESP_LOGI(TAG, "💓 bat=%u%% dsh=%s wifi=%s (IP=%s mode=%s)",
                 p4c5_pmic_get_battery_level(),
                 dsh_client_is_connected() ? "✅" : "❌",
                 wifi_st.sta_connected ? "connected" : "disconnected",
                 wifi_st.sta_ip ? wifi_st.sta_ip : "0.0.0.0",
                 wifi_st.mode ? wifi_st.mode : "off");

        /* 每 30s 扫描一次附近 AP 看实际能看到什么 */
        static int scan_count = 0;
        if (++scan_count % 3 == 0) {
            wifi_manager_scan_record_t records[10];
            uint16_t count = 0;
            if (wifi_manager_scan_aps(records, 10, &count) == ESP_OK && count > 0) {
                ESP_LOGI(TAG, "📡 Found %d APs:", count);
                for (int i = 0; i < count && i < 10; i++) {
                    ESP_LOGI(TAG, "  [%d] SSID='%s' RSSI=%d auth=%d ch=%d",
                             i, records[i].ssid, records[i].rssi,
                             records[i].authmode, records[i].primary);
                }
            } else {
                ESP_LOGW(TAG, "📡 Scan failed or no AP found");
            }
        }
    }
}
