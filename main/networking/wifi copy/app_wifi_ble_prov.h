#ifndef APP_WIFI_H
#define APP_WIFI_H

#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>
#include <freertos/timers.h>
#include "esp_pm.h"

#include "qrcode.h"

#include <freertos/semphr.h>

#include <driver/gpio.h>

#define BUTTON_GPIO GPIO_NUM_9

#define DEFAULT_LISTEN_INTERVAL 3
#define DEFAULT_BEACON_TIMEOUT 6
#define DEFAULT_PS_MODE WIFI_PS_MAX_MODEM

#define CONFIG_EXAMPLE_PROV_SECURITY_VERSION_2 1
#define CONFIG_EXAMPLE_PROV_SEC2_DEV_MODE 1

#define CONFIG_EXAMPLE_PROV_TRANSPORT 2


#define EXAMPLE_PROV_SEC2_USERNAME "wifiprov"
#define EXAMPLE_PROV_SEC2_PWD "abcd1234"

#define PROV_QR_VERSION "v1"
#define PROV_TRANSPORT_BLE "ble"
#define QRCODE_BASE_URL "https://espressif.github.io/esp-jumpstart/qrcode.html"

extern uint8_t wifi_mesh_channel;

void app_wifi_start(void);
esp_err_t reset_wifi_credentials(void);
uint8_t get_wifi_connection_channel();

#endif /* APP_WIFI_H */
