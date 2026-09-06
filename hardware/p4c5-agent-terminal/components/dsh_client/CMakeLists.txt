# dsh_client component — DSH JSON-over-WebSocket 协议层
idf_component_register(
    SRCS
        "dsh_client.c"
        "dsh_client_ws.c"
        "dsh_client_frames.c"
        "dsh_client_heartbeat.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_websocket_client
        esp_event
        esp_netif
        tcp_transport
        json
)
