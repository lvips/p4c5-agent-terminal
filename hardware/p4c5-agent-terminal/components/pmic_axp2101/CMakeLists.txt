idf_component_register(
    SRCS "axp2101.c" "axp2101_registers.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2c esp_driver_gpio freertos
)
