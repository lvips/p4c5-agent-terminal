/*
 * KSDIY - ESP32-P4 / ESP32-C5 development boards
 * Author: Kevincoooool - https://github.com/kevincoooool
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "lvgl.h"
#include "ksdiy_lvgl_font.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KSDIY_LCD_H_RES                    480
#define KSDIY_LCD_V_RES                    800

#if LVGL_VERSION_MAJOR >= 9
typedef lv_display_t ksdiy_lvgl_display_t;
#else
typedef lv_disp_t ksdiy_lvgl_display_t;
#endif

/** 仅初始化 MIPI/ST7102 面板，不启动 LVGL（供 PPA 直绘等场景） */
void ksdiy_panel_bare_init(void);
/** 初始化 ST7123 触摸（不依赖 LVGL） */
esp_err_t ksdiy_touch_bare_init(void);
void ksdiy_lvgl_port_init(void);
bool ksdiy_lvgl_lock(int timeout_ms);
void ksdiy_lvgl_unlock(void);
bool ksdiy_lvgl_is_ready(void);
ksdiy_lvgl_display_t *ksdiy_lvgl_get_display(void);
esp_lcd_panel_handle_t ksdiy_lvgl_get_panel_handle(void);
esp_lcd_touch_handle_t ksdiy_touch_get_handle(void);
extern i2c_master_bus_handle_t touch_i2c_bus_;

/** LCD backlight on GPIO6 (LEDC PWM). brightness: 0=off, 255=max */
esp_err_t ksdiy_lcd_backlight_init(void);
esp_err_t ksdiy_lcd_set_brightness(uint8_t brightness);
uint8_t ksdiy_lcd_get_brightness(void);

/* 注册库通用名兼容（组件目录名 lvgl_st7102_display） */
#define lvgl_st7102_set_brightness      ksdiy_lcd_set_brightness
#define lvgl_st7102_get_brightness      ksdiy_lcd_get_brightness
#define lvgl_st7102_port_init           ksdiy_lvgl_port_init
#define lvgl_st7102_lock                ksdiy_lvgl_lock
#define lvgl_st7102_unlock              ksdiy_lvgl_unlock
#define lvgl_st7102_is_ready            ksdiy_lvgl_is_ready
#define lvgl_st7102_get_display         ksdiy_lvgl_get_display
#define lvgl_st7102_get_panel_handle    ksdiy_lvgl_get_panel_handle
typedef ksdiy_lvgl_display_t lvgl_st7102_display_t;

#ifdef __cplusplus
}
#endif
