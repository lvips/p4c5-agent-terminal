# p4c5_ui — LVGL v9 UI via espressif/esp_lvgl_adapter
#
# T14: Initial production UI (hand-rolled 3-task model)
# T16: Switched to esp_lvgl_adapter for proper MIPI DSI VSYNC handling.
#   - esp_lv_adapter manages LVGL lifecycle (init/tick/timer/flush/touch)
#   - p4c5_display creates panel with num_fbs=3 (triple buffering)
#   - No PSRAM bandwidth contention between DSI stream and LVGL DMA
#
# Dependencies:
#   - espressif__esp_lvgl_adapter: manages LVGL + flush + VSYNC
#   - p4c5_display: provides panel/panel_io/touch handles
#   - p4c5_pmic / p4c5_4g / dsh_client: status sources for UI update

idf_component_register(
    SRCS "p4c5_ui.c"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        esp_lcd
        esp_lcd_touch
        espressif__esp_lvgl_adapter
        esp_driver_gpio
        freertos
        p4c5_board
        p4c5_pmic
        # p4c5_4g  # W1: 4G 搁置 (需电池)
        p4c5_display
        dsh_client
    PRIV_REQUIRES
        main
)
