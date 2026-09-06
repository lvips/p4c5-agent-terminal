# p4c5_display component
# 注: esp_ldo_regulator 在 IDF 5.5+ 已移入 esp_hw_support
# PRIV_REQUIRES main 引入 config.h (LCD/触摸引脚定义)
# T13: 添加 p4c5_board (I2C bus handle for touch) + esp_driver_i2c
idf_component_register(
    SRCS "p4c5_display.cc"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_gpio
        esp_driver_ledc
        esp_driver_i2c
        esp_lcd
        esp_lcd_touch
        esp_lcd_st7102
        esp_lcd_touch_st7123
        esp_hw_support
        p4c5_board
    PRIV_REQUIRES
        main
)
