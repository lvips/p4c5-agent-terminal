idf_component_register(
    SRCS "opus_encoder.cpp"
    INCLUDE_DIRS "include"
    REQUIRES freertos
    PRIV_REQUIRES esp-opus
)
