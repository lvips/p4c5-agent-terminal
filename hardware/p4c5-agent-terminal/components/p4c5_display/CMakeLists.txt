# p4c5_display component
idf_component_register(
    SRCS "p4c5_display.cc"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_gpio
        esp_driver_ledc
        esp_lcd
        esp_lcd_st7102
        esp_lcd_touch_st7123
        esp_ldo_regulator
)
