# p4c5_ui — minimal LVGL v9 UI for p4c5-agent-terminal
#
# T14: Production UI replacing the 5-band test pattern.
# Reuses esp_lcd_panel_handle_t + esp_lcd_touch_handle_t from p4c5_display.
# Does NOT re-initialize the LCD or touch — just wraps them for LVGL.
#
# LVGL APIs used (v9.4.0):
#   lv_display_create / set_flush_cb / set_buffers / set_color_format
#   lv_indev_create / set_type / set_user_data / set_read_cb
#   lv_obj / lv_label / lv_btn / lv_timer / lv_tick_inc
#   lv_mutex (FreeRTOS-backed)
#
# Thread model:
#   - All LVGL calls (except lv_tick_inc) are serialized via the same mutex
#     that protects the panel draw in the flush callback.
#   - lv_tick_inc(5) is called from the 5 ms tick task (no LVGL API needed).
#   - The flush callback runs on LVGL's internal render task; it draws via
#     esp_lcd_panel_draw_bitmap (thread-safe at the esp_lcd layer).

idf_component_register(
    SRCS "p4c5_ui.c"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        esp_lcd
        esp_lcd_touch
        esp_driver_gpio
        freertos
        p4c5_board
        p4c5_pmic
        p4c5_4g
        p4c5_display
        dsh_client
    PRIV_REQUIRES
        main
)
