/**
 * @file p4c5_ui.c
 * @brief T14 — 生产级 LVGL v9 UI 实现
 *
 * 设计：
 *   - 复用 p4c5_display 已初始化的 esp_lcd_panel_handle_t 和
 *     esp_lcd_touch_handle_t（不重新初始化 LCD / 触摸）
 *   - LVGL v9 API：lv_display / lv_indev / lv_obj
 *   - 所有 LVGL UI 写操作经同一 FreeRTOS mutex 串行化
 *   - lv_tick_inc(5) 由独立 5ms tick task 驱动（无锁，lv_tick_inc 线程安全）
 *   - lv_timer_handler() 由独立 task 驱动，锁内调用
 *
 * 线程模型：
 *   - Task A: lvgl_timer (优先级 2)
 *       lock -> lv_timer_handler() -> flush_cb -> unlock
 *   - Task B: ui_update (优先级 3, 1s 周期)
 *       lock -> lv_label_set_text* -> unlock
 *   - Task C: lvgl_tick (最高优先级, 5ms 周期)
 *       lv_tick_inc(5)  // 无锁，官方声明线程安全
 *
 * 外部调用 p4c5_ui_set_message/set_status 也走锁。
 */

#include "p4c5_ui.h"

#include "p4c5_board.h"
#include "p4c5_display.h"
#include "p4c5_pmic.h"
#include "p4c5_4g.h"
#include "dsh_client.h"
#include "config.h"

#include <esp_log.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_touch.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "p4c5_ui";

/* ── 内部状态 ── */
static lv_display_t         *s_disp        = NULL;
static lv_indev_t           *s_touch_in    = NULL;
static esp_lcd_panel_handle_t s_panel      = NULL;
static esp_lcd_touch_handle_t s_touch      = NULL;
static SemaphoreHandle_t     s_lvgl_mutex  = NULL;
static bool                  s_ui_ready    = false;

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
/* Partial buffer: 40 lines in internal SRAM (480*40*2 = 38.4KB)
 * 选择内部 SRAM 避免与 DSI 流式读取 PSRAM 争带宽。
 * LVGL 渲染到 SRAM → flush cb 通过 DMA 复制到面板 (面板仍从 PSRAM 流读)。
 * 40 行 ≈ 每帧 ~150 KB DMA，PSRAM 带宽足够。 */
#define UI_BUF_LINES     40
#define UI_UPDATE_PERIOD 1000                /* 1s 刷新一次状态 */
#define UI_TICK_PERIOD   5                   /* 5 ms tick */

/* ── 颜色（RGB565 友好） ── */
#define COL_BG           lv_color_hex(0x1a1a1a)
#define COL_FG           lv_color_hex(0xe0e0e0)
#define COL_ACCENT       lv_color_hex(0x00ff88)
#define COL_BTN_IDLE     lv_color_hex(0x0088ff)
#define COL_BTN_LISTEN   lv_color_hex(0xff0088)
#define COL_STATUS       lv_color_hex(0xaaaaaa)
#define COL_MSG_BG       lv_color_hex(0x262626)

/* ── 前向声明 ── */
static void lvgl_timer_task(void *arg);
static void lvgl_tick_task(void *arg);
static void ui_update_task(void *arg);

/* ════════════════════════════════════════════════════════════════
 * LVGL flush callback
 * LVGL render task 调用；把像素数据通过 esp_lcd 写到屏幕。
 * 注意：在 s_lvgl_mutex 锁内调用（由 lvgl_timer_task 持有）。
 * ════════════════════════════════════════════════════════════════ */
static void ui_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    if (!s_panel) {
        lv_display_flush_ready(disp);
        return;
    }
    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;
    esp_lcd_panel_draw_bitmap(s_panel,
                              area->x1, area->y1,
                              area->x1 + w, area->y1 + h,
                              px_map);
    lv_display_flush_ready(disp);
}

/* ════════════════════════════════════════════════════════════════
 * 触摸 read callback
 * LVGL 定时器 task 调用（在锁内），从 ST7123 读坐标。
 * 调用栈：lvgl_timer_task(locked) -> lv_timer_handler -> indev_read -> 这里
 * ════════════════════════════════════════════════════════════════ */
static void ui_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    if (!s_touch) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    if (esp_lcd_touch_read_data(s_touch) != ESP_OK) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    uint16_t x = 0, y = 0;
    uint16_t strength = 0;
    uint8_t  point_num = 0;
    /* 读坐标 — 使用 deprecated esp_lcd_touch_get_coordinates 兼容当前组件版本 */
    if (!esp_lcd_touch_get_coordinates(s_touch, &x, &y, &strength, &point_num, 1)) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    if (point_num == 0) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
}

/* ════════════════════════════════════════════════════════════════
 * 按钮事件：按下 = 开始监听，松开 = 停止监听
 * 在锁内调用（LVGL event callback 由 timer task 派发）
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
 * UI 控件创建（在 s_disp / s_touch_in 建立后，锁内调用）
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
 * 后台任务
 * ════════════════════════════════════════════════════════════════ */

/* LVGL timer handler — 优先级 2，低于 ui_update 但高于 tick */
static void lvgl_timer_task(void *arg)
{
    while (1) {
        if (p4c5_ui_lock_with_timeout(200)) {
            uint32_t idle = lv_timer_handler();
            p4c5_ui_unlock();
            if (idle > 100) idle = 100;
            if (idle < 5)   idle = 5;
            vTaskDelay(pdMS_TO_TICKS(idle));
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

/* 1 秒状态刷新 */
static void ui_update_task(void *arg)
{
    char buf[128];
    /* 等 UI 初始化完成 */
    while (!s_ui_ready) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    while (1) {
        if (p4c5_ui_lock_with_timeout(100)) {
            /* 电量 */
            if (s_lbl_bat) {
                uint8_t bat = p4c5_pmic_get_battery_level();
                snprintf(buf, sizeof(buf), "Battery: %u%%", bat);
                lv_label_set_text(s_lbl_bat, buf);
            }
            /* 4G */
            if (s_lbl_4g) {
                int csq  = p4c5_4g_get_csq();
                int rssi = p4c5_4g_get_rssi_dbm();
                bool ok  = p4c5_4g_is_network_ready();
                snprintf(buf, sizeof(buf), "4G: %ddBm (CSQ=%d) %s",
                         rssi, csq, ok ? "[OK]" : "[..]");
                lv_label_set_text(s_lbl_4g, buf);
            }
            /* DSH */
            if (s_lbl_dsh) {
                bool conn = dsh_client_is_connected();
                snprintf(buf, sizeof(buf), "DSH: %s",
                         conn ? "Connected [OK]" : "Disconnected");
                lv_label_set_text(s_lbl_dsh, buf);
            }
            p4c5_ui_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(UI_UPDATE_PERIOD));
    }
}

/* 5 ms tick — 最高优先级，无锁（lv_tick_inc 官方声明线程安全） */
static void lvgl_tick_task(void *arg)
{
    while (1) {
        lv_tick_inc(UI_TICK_PERIOD);
        vTaskDelay(pdMS_TO_TICKS(UI_TICK_PERIOD));
    }
}

/* ════════════════════════════════════════════════════════════════
 * 公开 API
 * ════════════════════════════════════════════════════════════════ */

void p4c5_ui_lock(void)
{
    if (s_lvgl_mutex) xSemaphoreTake(s_lvgl_mutex, portMAX_DELAY);
}

bool p4c5_ui_lock_with_timeout(uint32_t timeout_ms)
{
    if (!s_lvgl_mutex) return false;
    return xSemaphoreTake(s_lvgl_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void p4c5_ui_unlock(void)
{
    if (s_lvgl_mutex) xSemaphoreGive(s_lvgl_mutex);
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
    s_panel = (esp_lcd_panel_handle_t)p4c5_display_get_panel();
    s_touch = (esp_lcd_touch_handle_t)p4c5_display_get_touch();
    if (!s_panel) {
        ESP_LOGE(TAG, "Display panel not available (p4c5_display not init?)");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "Panel handle: %p, Touch handle: %p", s_panel, s_touch);

    /* 1. LVGL 初始化 */
    lv_init();
    ESP_LOGI(TAG, "lv_init() done (LVGL %d.%d.%d)",
             LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);

    /* 2. 互斥锁 */
    s_lvgl_mutex = xSemaphoreCreateMutex();
    if (!s_lvgl_mutex) {
        ESP_LOGE(TAG, "mutex alloc failed");
        return ESP_ERR_NO_MEM;
    }

    /* 3. Display — RGB565 部分 buffer */
    s_disp = lv_display_create(UI_WIDTH, UI_HEIGHT);
    if (!s_disp) {
        ESP_LOGE(TAG, "lv_display_create failed");
        return ESP_ERR_NO_MEM;
    }
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);

    /* 双 buffer 放在内部 SRAM，避免与 DSI 流式读 PSRAM 争带宽
     * 40 行 = 480*40*2 = 38.4 KB each, 2 个 = 76.8 KB (内部 SRAM 够) */
    const size_t buf_size = UI_WIDTH * UI_BUF_LINES * sizeof(lv_color16_t);
    void *buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    void *buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!buf1 || !buf2) {
        ESP_LOGE(TAG, "LVGL buf alloc failed (need %u bytes internal)", (unsigned)buf_size);
        if (buf1) free(buf1);
        if (buf2) free(buf2);
        return ESP_ERR_NO_MEM;
    }
    lv_display_set_buffers(s_disp, buf1, buf2, buf_size,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, ui_flush_cb);
    /* 降低 flush 频率（默认 33ms=30fps），改 100ms=10fps 降低 PSRAM 争带宽 */
    lv_timer_t *refr_timer = lv_display_get_refr_timer(s_disp);
    if (refr_timer) {
        lv_timer_set_period(refr_timer, 100);
    }
    ESP_LOGI(TAG, "LVGL display: %dx%d, 2x %u byte bufs (internal SRAM), 100ms refr",
             UI_WIDTH, UI_HEIGHT, (unsigned)buf_size);

    /* 4. 触摸 input device */
    if (s_touch) {
        s_touch_in = lv_indev_create();
        if (s_touch_in) {
            lv_indev_set_type(s_touch_in, LV_INDEV_TYPE_POINTER);
            lv_indev_set_read_cb(s_touch_in, ui_touch_read_cb);
            lv_indev_set_user_data(s_touch_in, s_touch);
            ESP_LOGI(TAG, "LVGL touch indev registered");
        } else {
            ESP_LOGW(TAG, "lv_indev_create failed");
        }
    } else {
        ESP_LOGW(TAG, "Touch not available; UI will be static");
    }

    /* 5. UI 控件（锁内） */
    p4c5_ui_lock();
    ui_create();
    p4c5_ui_unlock();
    s_ui_ready = true;

    /* 6. 启动后台任务 */
    /* tick task：最高优先级，保证 LVGL 时间基准稳定 */
    xTaskCreate(lvgl_tick_task, "lvgl_tick", 2048, NULL, configMAX_PRIORITIES - 1, NULL);
    /* timer task：处理 LVGL 动画、flush、事件派发 */
    xTaskCreate(lvgl_timer_task, "lvgl_timer", 6144, NULL, 2, NULL);
    /* status update：1s 周期，优先级略高 */
    xTaskCreate(ui_update_task, "ui_update", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "p4c5_ui initialized ✅ (3 tasks: tick/timer/update)");
    return ESP_OK;
}
