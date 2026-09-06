# main component CMakeLists.txt
# 最小入口，依赖 p4c5_board 统一初始化

set(SOURCES "app_main.c")

idf_component_register(SRCS ${SOURCES}
    INCLUDE_DIRS "."
    REQUIRES
        p4c5_board
        p4c5_pmic
        p4c5_display
        p4c5_audio
        p4c5_4g
        p4c5_ui
        dsh_client
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_i2s
        esp_driver_spi
        esp_lcd
        esp_lcd_st7102
        pmic_axp2101
        esp_audio_codec
)
