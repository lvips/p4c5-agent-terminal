/*
 * KSDIY - ESP32-P4 / ESP32-C5 development boards
 * Author: Kevincoooool - https://github.com/kevincoooool
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ksdiy_example_display.h"

#include <string.h>

#include "ksdiy_lvgl_font.h"
#include "ksdiy_lvgl_port.h"
#include "lvgl.h"

static lv_obj_t *s_title;
static lv_obj_t *s_subtitle;
static lv_obj_t *s_line1;
static lv_obj_t *s_line2;
static lv_obj_t *s_line3;

static void set_label_text(lv_obj_t *label, const char *text)
{
    if (label == NULL) {
        return;
    }
    lv_label_set_text(label, text != NULL ? text : "");
}

static lv_obj_t *create_text_label(lv_obj_t *parent, lv_align_t align, int x, int y, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_width(label, 220);
    lv_obj_align(label, align, x, y);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    ksdiy_lvgl_apply_font(label);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    return label;
}

void ksdiy_example_display_bootstrap(const char *title, const char *subtitle)
{
    ksdiy_lvgl_port_init();
    if (!ksdiy_lvgl_lock(1000)) {
        return;
    }

#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t *screen = lv_screen_active();
#else
    lv_obj_t *screen = lv_scr_act();
#endif
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101418), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    s_title = create_text_label(screen, LV_ALIGN_TOP_LEFT, 12, 16, 0xF2F5F8);
    s_subtitle = create_text_label(screen, LV_ALIGN_TOP_LEFT, 12, 48, 0x8BA0B3);
    s_line1 = create_text_label(screen, LV_ALIGN_TOP_LEFT, 12, 96, 0xD7E0E8);
    s_line2 = create_text_label(screen, LV_ALIGN_TOP_LEFT, 12, 126, 0xD7E0E8);
    s_line3 = create_text_label(screen, LV_ALIGN_TOP_LEFT, 12, 156, 0xD7E0E8);

    set_label_text(s_title, title);
    set_label_text(s_subtitle, subtitle);
    set_label_text(s_line1, "Display ready");
    set_label_text(s_line2, "");
    set_label_text(s_line3, "");

    ksdiy_lvgl_unlock();
}

void ksdiy_example_display_set_lines(const char *line1, const char *line2, const char *line3)
{
    if (!ksdiy_lvgl_lock(1000)) {
        return;
    }

    set_label_text(s_line1, line1);
    set_label_text(s_line2, line2);
    set_label_text(s_line3, line3);

    ksdiy_lvgl_unlock();
}
