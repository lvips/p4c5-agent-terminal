/**
 * @file p4c5_ui.c
 * @brief T16 — 生产级 LVGL v9 UI（基于 espressif/esp_lvgl_adapter）
 *
 * 设计：
 *   - 使用 esp_lvgl_adapter 组件统一管理 LVGL 生命周期
 *     （替代 T15 的手写 3-task 模型）
 *   - esp_lv_adapter 自动处理：
 *     * lv_init()
 *     * lv_tick_inc()
 *     * lv_timer_handler()
 *     * flush_cb（内部实现 VSYNC 同步 + multi-FB 切换）
 *     * 触摸 read_cb（通过 esp_lcd_touch 标准 API）
 *     * 线程安全锁
 *   - DSI underrun 根治：
 *     * p4c5_display 创建面板时 num_fbs = 3（三缓冲，见 p4c5_display.cc）
 *     * esp_lv_adapter 以 TRIPLE_PARTIAL 模式渲染
 *     * DSI 持续读 FB[i]，LVGL 渲染到 FB[j] (i≠j)，无带宽争用
 *
 * 线程模型：
 *   - esp_lv_adapter 内部创建一个 LVGL task（默认 8KB stack, priority 6）
 *     负责 tick + timer + flush + touch polling
 *   - ui_update task（我们创建，1s 周期，优先级 3）
 *     经 esp_lv_adapter_lock 后更新 label 文字
 *
 * 外部 API（p4c5_ui_set_message/set_status）也走 adapter lock。
 */

#include "p4c5_ui.h"

#include "p4c5_board.h"
#include "p4c5_display.h"
#include "p4c5_pmic.h"
// #include "p4c5_4g.h"  // W1: 4G 搁置 (需电池)
#include "dsh_client.h"
#include "config.h"

#include <esp_log.h>
#include <esp_check.h>
#include <esp_lcd_touch.h>
#include <esp_lv_adapter.h>
#include <lvgl.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "p4c5_ui";

/* ── 内部状态 ── */
static lv_display_t           *s_disp    = NULL;
static lv_indev_t             *s_touch_in = NULL;
static esp_lcd_panel_handle_t  s_panel   = NULL;
static esp_lcd_panel_io_handle_t s_panel_io = NULL;
static esp_lcd_touch_handle_t  s_touch   = NULL;
static bool                    s_ui_ready = false;

/* ── UI 控件句柄 ── */
static lv_obj_t *s_lbl_bat    = NULL;
static lv_obj_t *s_lbl_4g     = NULL;
static lv_obj_t *s_lbl_dsh    = NULL;
static lv_obj_t *s_lbl_msg    = NULL;
static lv_obj_t *s_btn_talk   = NULL;
static lv_obj_t *s_lbl_status = NULL;

/* ── 配置 ── */
#define UI_WIDTH         P4C5_LCD_WIDTH      /* 480 */
#define UI_HEIGHT        P4C5_LCD_HEIGHT     /* 800 */
#define UI_UPDATE_PERIOD 1000                /* 1s 刷新一次状态 */

/* ── 颜色（RGB565 友好） ── */
#define COL_BG           lv_color_hex(0x1a1a1a)
#define COL_FG           lv_color_hex(0xe0e0e0)
#define COL_ACCENT       lv_color_hex(0x00ff88)
#define COL_BTN_IDLE     lv_color_hex(0x0088ff)
#define COL_BTN_LISTEN   lv_color_hex(0xff0088)
#define COL_STATUS       lv_color_hex(0xaaaaaa)
#define COL_MSG_BG       lv_color_hex(0x262626)

/* ── 前向声明 ── */
static void ui_update_task(void *arg);

/* ════════════════════════════════════════════════════════════════
 * 按钮事件：按下 = 开始监听，松开 = 停止监听
 * 在 adapter lock 内调用（LVGL event callback 由 adapter task 派发）
 * ════════════════════════════════════════════════════════════════ */
static void btn_talk_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        ESP_LOGI(TAG, "[TALK] pressed");
        if (s_lbl_status) lv_label_set_text(s_lbl_status, "Status: Listening...");
        if (s_btn_talk)   lv_obj_set_style_bg_color(s_btn_talk, COL_BTN_LISTEN, LV_PART_MAIN);
        /* TODO: dsh_client_start_listening() */
    } else if (code == LV_EVENT_RELEASED) {
        ESP_LOGI(TAG, "[TALK] released");
        if (s_lbl_status) lv_label_set_text(s_lbl_status, "Status: Processing...");
        if (s_btn_talk)   lv_obj_set_style_bg_color(s_btn_talk, COL_BTN_IDLE, LV_PART_MAIN);
        /* TODO: dsh_client_stop_listening() */
    }
}

/* ════════════════════════════════════════════════════════════════
 * UI 控件创建（adapter 已建立 display/touch，锁内调用）
 * ════════════════════════════════════════════════════════════════ */
static void ui_create(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, COL_FG, 0);

    /* ── 标题栏 (y: 0-50) ── */
    lv_obj_t *lbl_title = lv_label_create(scr);
    lv_label_set_text(lbl_title, "p4c5-agent-terminal  v" P4C5_BOARD_VERSION);
    lv_obj_set_style_text_color(lbl_title, COL_ACCENT, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 15);

    /* 分隔线 */
    lv_obj_t *sep1 = lv_obj_create(scr);
    lv_obj_set_size(sep1, UI_WIDTH - 40, 1);
    lv_obj_set_style_bg_color(sep1, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(sep1, 0, 0);
    lv_obj_clear_flag(sep1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(sep1, LV_ALIGN_TOP_MID, 0, 50);

    /* ── 状态区 (y: 60-180) ── */
    s_lbl_bat = lv_label_create(scr);
    lv_label_set_text(s_lbl_bat, "Battery: --- %");
    lv_obj_align(s_lbl_bat, LV_ALIGN_TOP_LEFT, 20, 65);

    s_lbl_4g = lv_label_create(scr);
    lv_label_set_text(s_lbl_4g, "4G:  --- dBm  (init)");
    lv_obj_align(s_lbl_4g, LV_ALIGN_TOP_LEFT, 20, 95);

    s_lbl_dsh = lv_label_create(scr);
    lv_label_set_text(s_lbl_dsh, "DSH: disconnected");
    lv_obj_align(s_lbl_dsh, LV_ALIGN_TOP_LEFT, 20, 125);

    /* ── 消息区 (y: 170-430) ── */
    s_lbl_msg = lv_label_create(scr);
    lv_label_set_text(s_lbl_msg, "Last Assistant:\n  (no message yet)");
    lv_obj_set_style_bg_color(s_lbl_msg, COL_MSG_BG, 0);
    lv_obj_set_style_bg_opa(s_lbl_msg, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_lbl_msg, 12, 0);
    lv_obj_set_style_border_width(s_lbl_msg, 1, 0);
    lv_obj_set_style_border_color(s_lbl_msg, lv_color_hex(0x444444), 0);
    lv_obj_set_style_radius(s_lbl_msg, 8, 0);
    lv_obj_set_size(s_lbl_msg, UI_WIDTH - 40, 260);
    lv_label_set_long_mode(s_lbl_msg, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_lbl_msg, LV_ALIGN_TOP_LEFT, 20, 170);

    /* 分隔线 */
    lv_obj_t *sep2 = lv_obj_create(scr);
    lv_obj_set_size(sep2, UI_WIDTH - 40, 1);
    lv_obj_set_style_bg_color(sep2, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(sep2, 0, 0);
    lv_obj_clear_flag(sep2, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(sep2, LV_ALIGN_BOTTOM_MID, 0, -180);

    /* ── 大按钮 Hold to Talk (y: 520-640) ── */
    s_btn_talk = lv_btn_create(scr);
    lv_obj_set_size(s_btn_talk, 300, 100);
    lv_obj_set_style_bg_color(s_btn_talk, COL_BTN_IDLE, LV_PART_MAIN);
    lv_obj_set_style_radius(s_btn_talk, 16, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_btn_talk, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_btn_talk, LV_ALIGN_BOTTOM_MID, 0, -230);
    lv_obj_add_event_cb(s_btn_talk, btn_talk_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *lbl_btn = lv_label_create(s_btn_talk);
    lv_label_set_text(lbl_btn, "Hold to Talk");
    lv_obj_center(lbl_btn);

    /* ── 底部状态条 (y: 760-800) ── */
    s_lbl_status = lv_label_create(scr);
    lv_label_set_text(s_lbl_status, "Status: idle");
    lv_obj_set_style_text_color(s_lbl_status, COL_STATUS, 0);
    lv_obj_align(s_lbl_status, LV_ALIGN_BOTTOM_MID, 0, -30);

    ESP_LOGI(TAG, "UI created: title/bat/4g/dsh/msg/btn/status");
}

/* ════════════════════════════════════════════════════════════════
 * 后台任务：1 秒状态刷新
 * ════════════════════════════════════════════════════════════════ */
static void ui_update_task(void *arg)
{
    char buf[128];
    /* 等 UI 初始化完成 */
    while (!s_ui_ready) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    while (1) {
        if (esp_lv_adapter_lock(100) == ESP_OK) {
            /* 电量 */
            if (s_lbl_bat) {
                uint8_t bat = p4c5_pmic_get_battery_level();
                snprintf(buf, sizeof(buf), "Battery: %u%%", bat);
                lv_label_set_text(s_lbl_bat, buf);
            }
            /* WiFi (W1: 4G 搁置，改用 WiFi) */
            if (s_lbl_4g) {
                // TODO: 从 wifi_manager 获取 RSSI
                int rssi = -99;
                snprintf(buf, sizeof(buf), "WiFi: %ddBm", rssi);
                lv_label_set_text(s_lbl_4g, buf);
            }
            /* DSH */
            if (s_lbl_dsh) {
                bool conn = dsh_client_is_connected();
                snprintf(buf, sizeof(buf), "DSH: %s",
                         conn ? "Connected [OK]" : "Disconnected");
                lv_label_set_text(s_lbl_dsh, buf);
            }
            esp_lv_adapter_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(UI_UPDATE_PERIOD));
    }
}

/* ════════════════════════════════════════════════════════════════
 * 公开 API（lock 直接走 esp_lv_adapter）
 * ════════════════════════════════════════════════════════════════ */

void p4c5_ui_lock(void)
{
    esp_lv_adapter_lock(-1);
}

bool p4c5_ui_lock_with_timeout(uint32_t timeout_ms)
{
    return esp_lv_adapter_lock((int32_t)timeout_ms) == ESP_OK;
}

void p4c5_ui_unlock(void)
{
    esp_lv_adapter_unlock();
}

void p4c5_ui_set_message(const char *text)
{
    if (!s_lbl_msg) return;
    p4c5_ui_lock();
    if (text) {
        char buf[512];
        snprintf(buf, sizeof(buf), "Last Assistant:\n  %s", text);
        lv_label_set_text(s_lbl_msg, buf);
    } else {
        lv_label_set_text(s_lbl_msg, "Last Assistant:\n  (no message yet)");
    }
    p4c5_ui_unlock();
}

void p4c5_ui_set_status(const char *text)
{
    if (!s_lbl_status) return;
    p4c5_ui_lock();
    if (text) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Status: %s", text);
        lv_label_set_text(s_lbl_status, buf);
    } else {
        lv_label_set_text(s_lbl_status, "Status: idle");
    }
    p4c5_ui_unlock();
}

/* ════════════════════════════════════════════════════════════════
 * 初始化入口
 * ════════════════════════════════════════════════════════════════ */
esp_err_t p4c5_ui_init(void)
{
    if (s_ui_ready) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    /* 从 p4c5_display 取已初始化的句柄 */
    s_panel   = (esp_lcd_panel_handle_t)p4c5_display_get_panel();
    s_panel_io = (esp_lcd_panel_io_handle_t)p4c5_display_get_panel_io();
    s_touch   = (esp_lcd_touch_handle_t)p4c5_display_get_touch();
    if (!s_panel) {
        ESP_LOGE(TAG, "Display panel not available (p4c5_display not init?)");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "Panel=%p, panel_io=%p, Touch=%p", s_panel, s_panel_io, s_touch);

    /* 1. 初始化 esp_lv_adapter（内部调用 lv_init） */
    esp_lv_adapter_config_t adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter_cfg.task_stack_size = 10 * 1024;   /* 10KB stack */
    adapter_cfg.task_priority   = 6;           /* 中等偏高 */
    adapter_cfg.stack_in_psram  = true;        /* 任务栈放 PSRAM 节省内部 RAM */
    ESP_RETURN_ON_ERROR(esp_lv_adapter_init(&adapter_cfg), TAG, "adapter init failed");
    ESP_LOGI(TAG, "esp_lv_adapter initialized (LVGL %d.%d.%d)",
             LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);

    /* 2. 注册 display — MIPI DSI 默认用 TRIPLE_PARTIAL tear-avoidance */
    esp_lv_adapter_display_config_t disp_cfg =
        ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(
            s_panel, s_panel_io, UI_WIDTH, UI_HEIGHT, ESP_LV_ADAPTER_ROTATE_0);
    s_disp = esp_lv_adapter_register_display(&disp_cfg);
    if (!s_disp) {
        ESP_LOGE(TAG, "register_display failed");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "LVGL display registered (MIPI DSI, TRIPLE_PARTIAL)");

    /* 3. 注册 touch input device */
    if (s_touch) {
        esp_lv_adapter_touch_config_t touch_cfg =
            ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(s_disp, s_touch);
        s_touch_in = esp_lv_adapter_register_touch(&touch_cfg);
        if (s_touch_in) {
            ESP_LOGI(TAG, "LVGL touch indev registered");
        } else {
            ESP_LOGW(TAG, "register_touch failed");
        }
    } else {
        ESP_LOGW(TAG, "Touch not available; UI will be static");
    }

    /* 4. 启动 adapter 内部 task (tick + timer + flush) */
    ESP_RETURN_ON_ERROR(esp_lv_adapter_start(), TAG, "adapter start failed");

    /* 5. UI 控件（adapter lock 内） */
    esp_lv_adapter_lock(-1);
    /* 设置 LVGL 默认主题（深蓝 + 红强调色） */
    lv_theme_t *theme = lv_theme_default_init(
        s_disp,
        lv_palette_main(LV_PALETTE_BLUE),
        lv_palette_main(LV_PALETTE_RED),
        true,                              /* light mode */
        &lv_font_montserrat_14);
    lv_display_set_theme(s_disp, theme);
    ui_create();
    esp_lv_adapter_unlock();
    s_ui_ready = true;

    /* 6. 启动状态刷新 task */
    xTaskCreate(ui_update_task, "ui_update", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "p4c5_ui initialized ✅ (esp_lv_adapter manages LVGL task)");
    return ESP_OK;
}
