# p4c5_4g component
# PRIV_REQUIRES main 引入 config.h (4G 引脚定义)
idf_component_register(
    SRCS "p4c5_4g.cc"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_gpio
        esp_driver_uart
    PRIV_REQUIRES
        main
)
