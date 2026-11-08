idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES BSP
                             esp_event
                             esp_http_server
                             esp_netif
                             esp_wifi
                             lwip
                             nvs_flash)