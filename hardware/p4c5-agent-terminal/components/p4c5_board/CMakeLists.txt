# p4c5_board component — 板级统一初始化
idf_component_register(
    SRCS "p4c5_board.cc"
    INCLUDE_DIRS "."
    REQUIRES
        p4c5_pmic
        p4c5_display
        p4c5_audio
        p4c5_4g
        esp_driver_i2c
)
