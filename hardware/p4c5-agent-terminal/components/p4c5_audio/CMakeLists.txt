# p4c5_audio component — 完整音频子系统
# ES8311 (DAC) + ES7210 (ADC) + NS4150B (PA) + I2S + AEC
# PRIV_REQUIRES main 引入 config.h (音频引脚定义)
idf_component_register(
    SRCS "p4c5_audio.cc"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_i2c
        esp_driver_i2s
        esp_audio_codec
        pmic_axp2101
        esp_codec_dev
    PRIV_REQUIRES
        main
)
