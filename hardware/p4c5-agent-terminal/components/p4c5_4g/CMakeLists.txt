# p4c5_4g component — ML307C 4G modem via esp-ml307
# ⚠️ T14 fix: esp-ml307 v3.5.3 bundles uart-uhci internally; don't require
# 78/uart-uhci separately (which needs IDF 5.5.2+).
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
)
