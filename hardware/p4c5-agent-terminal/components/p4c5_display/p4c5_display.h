/**
 * @file p4c5_display.h
 * @brief P4C5 显示子系统 — ST7102 MIPI-DSI LCD + ST7123 触摸
 *
 * 基于 xiaozhi kevin_p4c5_4g_board.cc 移植。
 * 使用 esp_lcd_st7102 组件 + esp_lcd_touch_st7123 组件。
 * 屏幕规格：480×800 RGB565, 2-lane MIPI-DSI, 820Mbps/lane
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化显示子系统
 *
 * 步骤：
 *   1. 配置 LCD RST GPIO
 *   2. 使能 MIPI DSI PHY LDO (channel 3, 2.5V)
 *   3. 创建 MIPI DSI bus (2 lane, 820Mbps)
 *   4. 初始化 ST7102 panel (DBI + DPI)
 *   5. 初始化 ST7123 触摸 (I2C, shared bus)
 *   6. 配置背光 PWM (GPIO6)
 *
 * @return ESP_OK 成功
 */
esp_err_t p4c5_display_init(void);

/**
 * @brief 设置背光亮度
 * @param level 0(灭) - 255(最亮)
 */
esp_err_t p4c5_display_bl_set(uint8_t level);

/**
 * @brief 获取当前背光亮度
 */
uint8_t p4c5_display_bl_get(void);

/**
 * @brief 开关屏幕
 */
esp_err_t p4c5_display_power_on(bool on);

/**
 * @brief 反初始化显示子系统
 */
void p4c5_display_deinit(void);

/**
 * @brief 获取 LCD panel handle（供 app_main 测试用）
 * @return panel handle，未初始化时返回 NULL
 */
void* p4c5_display_get_panel(void);

/**
 * @brief 获取 ST7123 触摸 handle
 * @return touch handle，未初始化或失败返回 NULL
 */
void* p4c5_display_get_touch(void);

#ifdef __cplusplus
}
#endif
