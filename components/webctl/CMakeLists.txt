idf_component_register(
    SRCS "wifi.cpp" "http.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_wifi esp_netif esp_event esp_http_server esp_timer json nvs_flash app_update esp_app_format animations panel
    EMBED_TXTFILES "web/index.html"
)
