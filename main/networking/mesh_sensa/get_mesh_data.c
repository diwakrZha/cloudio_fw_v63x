#include "get_mesh_data.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "messaging.h"
#include "cJSON.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "app_wifi.h"
#include "esp_wifi.h"

#include "nvs_data_ops.h"

#define SETUP_GPIO_NUM 9

static const char *TAG = "MESH_ROOT";

void check_and_clear_config(void);
static void operational_mode();

const uint8_t sensor_macs[][ESP_NOW_ETH_ALEN] = {
    {0x40, 0x4c, 0xca, 0x46, 0xb2, 0x74},
    {0x50, 0x4d, 0xcb, 0x56, 0xb3, 0x85},
    {0x60, 0x4e, 0xcc, 0x66, 0xb4, 0x96}
};

static int ap_channel = -1; // Initialize to an impossible value for debugging

bool config_exists = true;

void sense_mesh()
{
    if (!config_exists)
    {
        ESP_LOGI(TAG, "No configuration found, entering setup mode.");
    }
    else
    {
        ESP_LOGI(TAG, "Configuration found, entering operational mode.");
        operational_mode();
    }
}

static void operational_mode()
{
    initMessagingModule();
    // Setup messaging for ESP-NOW
    ESP_LOGW(TAG, "Setting up ESP-NOW");
    size_t sensor_mac_count = sizeof(sensor_macs) / sizeof(sensor_macs[0]);
    setupESPNow(mesh_sensor_data_handler, get_channel(&ap_channel), sensor_macs, sensor_mac_count);

    //setupESPNow(mesh_sensor_data_handler);
    // Create a task for sending sensor data
    //xTaskCreate(sensor_data_task, "sensor_data_task", 2048, NULL, 5, NULL);
}


void check_and_clear_config() {
    //gpio_pad_select_gpio(SETUP_GPIO_NUM);
    gpio_set_direction(SETUP_GPIO_NUM, GPIO_MODE_INPUT);
    gpio_set_pull_mode(SETUP_GPIO_NUM, GPIO_PULLUP_ONLY);

    if (gpio_get_level(SETUP_GPIO_NUM) == 0) {
        ESP_LOGI(TAG, "Setup button pressed, clearing configuration...");
        nvs_handle_t nvs_handle;
        ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &nvs_handle));
        ESP_ERROR_CHECK(nvs_erase_all(nvs_handle));
        ESP_ERROR_CHECK(nvs_commit(nvs_handle));
        nvs_close(nvs_handle);
        cleanupMessagingModule();
        ESP_LOGI(TAG, "Configuration cleared, rebooting...");
        esp_restart();
    }
}