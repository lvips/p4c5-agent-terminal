/**
 * @file p4c5_ui.h
 * @brief T14 — 生产级 LVGL UI for p4c5-agent-terminal
 *
 * 取代 app_main 中的 5 色带测试图。显示：
 *   - 标题栏
 *   - 电量 / 4G 信号 / DSH 状态
 *   - 消息区（assistant 回复等）
 *   - "Hold to Talk" 触摸按钮
 *   - 底部状态栏
 *
 * 设计要点：
 *   - 不重新初始化 LCD / 触摸 — 复用 p4c5_display 的 panel + touch handle
 *   - 480×800 RGB565, 深色背景 + 高亮文字
 *   - 所有 LVGL 调用经同一 mutex 串行化
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 LVGL + 创建 UI
 *
 * 前置条件：
 *   - p4c5_display_init() 已成功（panel + touch 可用）
 *   - p4c5_display_get_panel() / get_touch() 能返回非 NULL
 *
 * 内部行为：
 *   - lv_init()
 *   - 创建 lv_display，flush callback 调用 esp_lcd_panel_draw_bitmap
 *   - 创建 lv_indev（触摸），read callback 调用 esp_lcd_touch_read_data
 *   - 创建 UI 控件
 *   - 启动 1s 状态刷新 task
 *   - 启动 5ms tick task（lv_tick_inc）
 *
 * @return ESP_OK 成功
 */
esp_err_t p4c5_ui_init(void);

/**
 * @brief 设置消息区文字（assistant 回复等）
 * @param text UTF-8 字符串；传 NULL 清空
 */
void p4c5_ui_set_message(const char *text);

/**
 * @brief 设置底部状态栏文字
 */
void p4c5_ui_set_status(const char *text);

/**
 * @brief LVGL 互斥锁（供 app_main 等外部 task 调用 LVGL API 前使用）
 */
void p4c5_ui_lock(void);
void p4c5_ui_unlock(void);

/**
 * @brief 带超时的锁获取
 * @param timeout_ms 超时毫秒数
 * @return true 拿到锁，false 超时
 */
bool p4c5_ui_lock_with_timeout(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
