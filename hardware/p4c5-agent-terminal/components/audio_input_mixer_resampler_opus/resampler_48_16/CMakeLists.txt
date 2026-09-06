idf_component_register(
    SRCS "resampler_48_16.cpp"
    INCLUDE_DIRS "include"
    REQUIRES freertos
    PRIV_REQUIRES esp-dsp
)
