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
#include "p4c5_ui.h"
#include "dsh_client.h"
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

    /* [4] 4G (ML307C) — 后台异步初始化 */
    ESP_LOGI(TAG, "[4/5] 4G init (ML307C, baud=%d)...", P4C5_4G_BAUD_RATE);
    p4c5_4g_set_event_callback(on_4g_event, NULL);
    err = p4c5_4g_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "4G init failed (non-fatal): %s", esp_err_to_name(err));
    }

    /* ── 等待模组检测（最多 10s）─────────────────────────
     * ⚠️ 必须在循环内喂狗！系统 TWDT 超时=5s，不喂会 panic。
     * 如果模组检测成功，使用 ML307 transport；否则回退 WiFi transport。
     * ─────────────────────────────────────────────────────────── */
    ESP_LOGI(TAG, "Waiting for modem detect (max 10s)...");
    int wait_count = 0;
    while (!p4c5_4g_is_modem_detected() && wait_count < 10) {
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1000));
        wait_count++;
    }
    bool use_ml307 = p4c5_4g_is_modem_detected();
    ESP_LOGI(TAG, "Modem wait: %s after %ds",
             use_ml307 ? "DETECTED" : "TIMEOUT", wait_count);

    /* [5] DSH Client — 必须在 init 之前设置 transport */
    ESP_LOGI(TAG, "[5/5] DSH client init...");

    /* ⚠️ set_transport 必须在 init 之前调用 */
    if (use_ml307) {
        const dsh_transport_t *transport = p4c5_4g_get_transport();
        if (transport) {
            esp_err_t terr = dsh_client_set_transport(transport);
            if (terr == ESP_OK) {
                ESP_LOGI(TAG, "Using ML307 4G transport");
            } else {
                ESP_LOGW(TAG, "set_transport failed (%s), fallback to WiFi WS",
                         esp_err_to_name(terr));
            }
        }
    } else {
        ESP_LOGI(TAG, "Modem not found, using default WiFi WS transport");
    }

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

    /* 连接 DSH
     * ⚠️ 只在有可用网络时连接：
     *   - ML307 模组检测到 → 通过 4G 连接（即使网络还没注册，transport 内部会等）
     *   - 无 ML307 → WiFi transport 需要 esp_netif 初始化。
     *     当前 build 未包含 WiFi netif 初始化，连接会导致 lwip assert。
     *     待后续 WiFi 组件就绪后启用。
     */
    if (use_ml307) {
        err = dsh_client_connect();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "DSH connect via 4G failed (will retry): %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGW(TAG, "No network available (4G=❌, WiFi netif=not init). DSH connect deferred.");
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

        ESP_LOGI(TAG, "💓 bat=%u%% csq=%d rssi=%ddBm dsh=%s 4g=%s",
                 p4c5_pmic_get_battery_level(),
                 p4c5_4g_get_csq(),
                 p4c5_4g_get_rssi_dbm(),
                 dsh_client_is_connected() ? "✅" : "❌",
                 p4c5_4g_is_network_ready() ? "✅" : "⏳");
    }
}
