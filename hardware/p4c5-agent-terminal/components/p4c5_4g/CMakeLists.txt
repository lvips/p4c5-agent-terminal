# p4c5_4g component — ML307C 4G modem via esp-ml307
idf_component_register(
    SRCS "p4c5_4g.cc" "p4c5_4g_transport.cc"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_gpio
        esp_event
        dsh_client
    PRIV_REQUIRES
        main
        78__esp-ml307
        78__uart-uhci
)
