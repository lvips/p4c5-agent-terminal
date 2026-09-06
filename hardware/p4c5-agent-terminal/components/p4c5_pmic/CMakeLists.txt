# p4c5_pmic component
# PRIV_REQUIRES main 引入 config.h (PMIC 引脚定义)
idf_component_register(
    SRCS "p4c5_pmic.cc"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_i2c
        pmic_axp2101
    PRIV_REQUIRES
        main
)
