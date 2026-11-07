idf_component_register(
    SRCS "wifi.cpp" "http.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_wifi esp_netif esp_event esp_http_server esp_timer json nvs_flash app_update esp_app_format animations panel
    EMBED_TXTFILES "web/index.html"
    EMBED_FILES "web/icon.svg" "web/icon-180.png"
                "web/fonts/inter-400.woff2" "web/fonts/inter-600.woff2"
                "web/fonts/space-grotesk-500.woff2" "web/fonts/jetbrains-mono-400.woff2"
)
