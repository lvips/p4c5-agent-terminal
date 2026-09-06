# p4c5_display component
# 注: esp_ldo_regulator 在 IDF 5.5+ 已移入 esp_hw_support
# PRIV_REQUIRES main 引入 config.h (LCD/触摸引脚定义)
idf_component_register(
    SRCS "p4c5_display.cc"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_gpio
        esp_driver_ledc
        esp_lcd
        esp_lcd_st7102
        esp_lcd_touch_st7123
        esp_hw_support
    PRIV_REQUIRES
        main
)
