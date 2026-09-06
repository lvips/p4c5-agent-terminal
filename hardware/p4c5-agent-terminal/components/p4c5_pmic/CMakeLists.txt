# p4c5_pmic component
idf_component_register(
    SRCS "p4c5_pmic.cc"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_i2c
        pmic_axp2101
)
