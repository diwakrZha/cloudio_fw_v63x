set(MAIN_SRCS
   "ping_time.c"  
)

set(MAIN_INCLUDE_DIRS
    "include"
    
)

set(MAIN_REQUIRES
    wifi_prov_mgr
    driver
    nvs_flash
    esp_timer
    esp-tls
)

idf_component_register(
    SRCS
        ${MAIN_SRCS}
    INCLUDE_DIRS
        ${MAIN_INCLUDE_DIRS}
    REQUIRES
        ${MAIN_REQUIRES}
)