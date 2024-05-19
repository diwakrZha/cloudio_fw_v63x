set(MAIN_SRCS
   "app_wifi.c"
    
)

set(MAIN_INCLUDE_DIRS
    "include"
    
)

set(MAIN_REQUIRES
    qrcode
    wifi_provisioning
    unity
    nvs_flash
    driver
    esp_adc
)

idf_component_register(
    SRCS
        ${MAIN_SRCS}
    INCLUDE_DIRS
        ${MAIN_INCLUDE_DIRS}
    REQUIRES
        ${MAIN_REQUIRES}
)