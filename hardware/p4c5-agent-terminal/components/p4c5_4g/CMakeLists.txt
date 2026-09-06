# p4c5_4g component
idf_component_register(
    SRCS "p4c5_4g.cc"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_gpio
        esp_driver_uart
)
