set(MAIN_SRCS
    "main.c"

    #"networking/wifi/app_wifi_with_homekit.c"
    #"networking/wifi/app_homekit.c"
   #"networking/wifi/app_wifi.c"
    #"networking/wifi/vmesh.c"
    "networking/wifi/connect_spec.c"
    

    "networking/mqtt/subscription_manager.c"
    "networking/mqtt/core_mqtt_agent_manager.c"
    "networking/mqtt/core_mqtt_agent_manager_events.c"
    "networking/mesh_sensa/get_mesh_data.c"
    "networking/mesh_sensa/messaging.c"

    #"networking/modbus/mb_master.c"
    #"networking/modbus/mb_uart_test.c"
    #"networking/modbus/slave.c"
    "networking/modbus/modbus_params.c"


    "core/extra/device_config.c"
    "core/extra/button_press.c"
    "core/extra/led_strip_encoder.c"
    #"core/extra/buzzer.c"
    "core/extra/uart_handler.c"
    "core/extra/ldo_control.c"
    "core/extra/sensors.c"
    "core/extra/psm_routine.c"
    "core/extra/led.c"
    #"core/extra/get_time.c"
    #"core/extra/ping_time.c"
    "core/extra/json_payload.c"
    "core/extra/aws_config_handler.c"
    "core/extra/vinit.c"

    "core/libraries/processing/calc_median.c"
    "core/libraries/cjson/cJSON.c"
    "core/libraries/watchdog/dev_wdt.c"
    "core/libraries/device_sleep/dev_sleep.c"
    "core/libraries/chip_system/reset_reason.c"
    "core/libraries/nvs_ops/nvs_data_ops.c"
    
)

# Demo enables

# Sub Pub Unsub demo
if(CONFIG_GRI_ENABLE_SUB_PUB_UNSUB_DEMO)
    list(APPEND MAIN_SRCS "tasks/shadow_pub/shadow_pub.c")
endif()

# Temperature Sub Pub and LED control demo
if(SENSOR_TO_AWS)
    list(APPEND MAIN_SRCS 
        "tasks/shadow_pub/shadow_pub.c"
    )
endif()

# OTA demo
if(CONFIG_GRI_ENABLE_OTA_DEMO)
    list(APPEND MAIN_SRCS "tasks/ota_over_mqtt_demo/ota_mqtt.c")
endif()


set(MAIN_INCLUDE_DIRS
    "."
    "tasks/ota_over_mqtt_demo"
    "tasks/shadow_pub"
    
    "networking/wifi"
    "networking/mqtt"
    "networking/mesh_sensa"
        "networking/modbus"

    "core/libraries/processing"
    "core/libraries/cjson"
    "core/libraries/watchdog"
    "core/libraries/device_sleep"
    "core/libraries/chip_system"
    "core/libraries/nvs_ops"

    "core/libraries/processing"
    "core/include"
)

set(MAIN_REQUIRES
    qrcode
    wifi_prov_mgr
    #wifi_provisioning
    coreMQTT
    coreMQTT-Agent
    corePKCS11
    coreJSON
    backoffAlgorithm
    esp_secure_cert_mgr
    ota-for-aws-iot-embedded-sdk
    FreeRTOS-Libraries-Integration-Tests
    unity
    driver
    esp_http_server
    esp_adc
    get_time
    esp_http_client
    #mesh_sensa

    #    zh_network
    #    zh_vector
    #    zh_espnow
    #    
    #    esp_hap_core
    #    esp_hap_apple_profiles
    #    esp_hap_platform
    #    hkdf-sha
    #    mu_srp
    #    json_generator
    #    json_parser
)

set(EXCLUDE_COMPONENTS freemodbus)

idf_component_register(
    SRCS
        ${MAIN_SRCS}
    INCLUDE_DIRS
        ${MAIN_INCLUDE_DIRS}
    REQUIRES
        ${MAIN_REQUIRES}
)

# OTA demo
if( CONFIG_GRI_ENABLE_OTA_DEMO OR CONFIG_GRI_RUN_QUALIFICATION_TEST )
    target_add_binary_data(${COMPONENT_TARGET} "certs/aws_codesign.crt" TEXT)
endif()

# Root Certificate
target_add_binary_data(${COMPONENT_TARGET} "certs/root_cert_auth.crt" TEXT)
