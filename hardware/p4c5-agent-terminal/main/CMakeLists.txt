# main component CMakeLists.txt
# 最小入口，依赖 p4c5_board 统一初始化

set(SOURCES "app_main.cpp")

idf_component_register(SRCS ${SOURCES}
    INCLUDE_DIRS "."
    REQUIRES
        p4c5_board
        p4c5_pmic
        p4c5_display
        p4c5_audio
        # p4c5_4g  # W1: 4G 搁置 (需电池), 改用 WiFi
        wifi_manager
        p4c5_ui
        dsh_client
        audio_mixer                # W3: 4ch→1ch 混音
        aec_sw                     # W3: 软件 AEC 回声消除 (NLMS)
        resampler_24_16            # W3: 24kHz→16kHz 重采样
        opus_encoder               # W3: libopus 编码 (16kbps, 20ms 帧)
        tts_player                 # W3: TTS 下行 PCM 播放
        wake_word_detector         # W4: P4 自实现 RMS 唤醒检测
        nvs_flash
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_i2s
        esp_driver_spi
        esp_lcd
        esp_lcd_st7102
        pmic_axp2101
        esp_audio_codec
)
