idf_component_register(SRCS 
                            "ip_reporter.c" "rtc_sync.c" "socket_api_helper.c"
                        INCLUDE_DIRS 
                            "include"
                        PRIV_REQUIRES 
                            "lwip"  "driver"  "esp_wifi" "esp_netif" "json"
    )
