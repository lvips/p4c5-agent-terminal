# p4c5_audio component
idf_component_register(
    SRCS "p4c5_audio.cc"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_i2c
        esp_driver_i2s
        esp_audio_codec
        pmic_axp2101
)
