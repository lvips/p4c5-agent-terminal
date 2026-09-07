# audio_wake_word — ESP-SR WakeNet9 唤醒词检测 (P1)

idf_component_register(
    SRCS "audio_wake_word.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        esp-sr
        audio_afe_processor
)
