/*
 * KSDIY - ESP32-P4 / ESP32-C5 development boards
 * Author: Kevincoooool - https://github.com/kevincoooool
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "myFont.h"

#ifdef __cplusplus
extern "C" {
#endif

/** KSDIY 统一 UI 字体（myFont_24） */
#define KSDIY_UI_FONT (&myFont)

static inline void ksdiy_lvgl_apply_font(lv_obj_t *obj)
{
    if (obj) {
        lv_obj_set_style_text_font(obj, KSDIY_UI_FONT, LV_PART_MAIN);
    }
}

#ifdef __cplusplus
}
#endif
