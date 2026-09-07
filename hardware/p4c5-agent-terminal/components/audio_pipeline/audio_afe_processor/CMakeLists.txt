# audio_afe_processor — ESP-SR AFE 封装 (P1)
#
# 用 ESP-SR AFE handle 统一处理 AEC + NS + VAD + AGC
# 替换自实现 NLMS AEC + 24→16 resampler + RMS VAD

idf_component_register(
    SRCS "audio_afe_processor.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        esp-sr
        p4c5_audio
)
