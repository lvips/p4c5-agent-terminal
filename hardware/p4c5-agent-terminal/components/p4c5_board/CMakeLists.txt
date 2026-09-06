# p4c5_board component — 板级共享资源（仅 I2C bus）
# 不在 board 内调 p4c5_*_init()，由 main.c 直接调用（避免静态库链接符号问题）
# REQUIRES 仍保留以解决头文件依赖（p4c5_board.h 间接引用）
# PRIV_REQUIRES main 引入 config.h (I2C 引脚定义)
idf_component_register(
    SRCS "p4c5_board.cc"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_i2c
    PRIV_REQUIRES
        main
)
