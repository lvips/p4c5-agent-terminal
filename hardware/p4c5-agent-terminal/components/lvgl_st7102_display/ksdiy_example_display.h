/*
 * KSDIY - ESP32-P4 / ESP32-C5 development boards
 * Author: Kevincoooool - https://github.com/kevincoooool
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void ksdiy_example_display_bootstrap(const char *title, const char *subtitle);
void ksdiy_example_display_set_lines(const char *line1, const char *line2, const char *line3);

#define lvgl_st7102_display_bootstrap   ksdiy_example_display_bootstrap
#define lvgl_st7102_display_set_lines   ksdiy_example_display_set_lines

#ifdef __cplusplus
}
#endif
