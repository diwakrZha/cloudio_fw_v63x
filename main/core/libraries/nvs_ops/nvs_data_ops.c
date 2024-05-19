#include "nvs_data_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"

#define SETUP_GPIO_NUM 9
static const char *TAG = "CONNECT_SPEC_SAVE_NVS";

// Function prototypes for saving and retrieving config
static esp_err_t save_integer_to_nvs(const char *key, int value);
static esp_err_t save_string_to_nvs(const char *key, const char *value);

static esp_err_t get_blob_from_nvs(const char *key, void *value, size_t *expected_size);
static esp_err_t get_string_from_nvs(const char *key, char *value, size_t max_len);
static esp_err_t get_integer_from_nvs(const char *key, int *value);

esp_err_t save_root_mac(const char *root_mac_str);
esp_err_t save_channel(int channel);
esp_err_t save_rssi(int rssi);
esp_err_t save_ap_mac(const char *ap_mac);
esp_err_t save_ip_address(const char *ip_address);

esp_err_t get_root_mac(uint8_t *root_mac);
esp_err_t get_channel(int *channel);
esp_err_t get_rssi(int *rssi);
esp_err_t get_ap_mac(char *ap_mac, size_t max_len);
esp_err_t get_ip_address(char *ip_address, size_t max_len);

// Implementation of save functions
esp_err_t save_root_mac(const char *root_mac_str)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
        return err;

    uint8_t root_mac[6];
    sscanf(root_mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &root_mac[0], &root_mac[1], &root_mac[2], &root_mac[3], &root_mac[4], &root_mac[5]);
    err = nvs_set_blob(nvs_handle, "root_mac", root_mac, sizeof(root_mac));

    if (err == ESP_OK)
    {
        err = nvs_commit(nvs_handle);
    }
    nvs_close(nvs_handle);
    return err;
}

esp_err_t save_channel(int channel)
{
    return save_integer_to_nvs("channel", channel);
}

esp_err_t save_rssi(int rssi)
{
    return save_integer_to_nvs("rssi", rssi);
}

esp_err_t save_ap_mac(const char *ap_mac)
{
    return save_string_to_nvs("ap_mac", ap_mac);
}

esp_err_t save_ip_address(const char *ip_address)
{
    return save_string_to_nvs("ip_address", ip_address);
}

// Generic save functions to reduce code duplication
static esp_err_t save_integer_to_nvs(const char *key, int value)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS for key %s with error: %s", key, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Saving integer %d to NVS with key %s", value, key);
    err = nvs_set_i32(nvs_handle, key, value);
    if (err == ESP_OK)
    {
        err = nvs_commit(nvs_handle);
        ESP_LOGI(TAG, "NVS commit: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGE(TAG, "Failed to save integer to NVS with error: %s", esp_err_to_name(err));
    }
    nvs_close(nvs_handle);
    return err;
}

static esp_err_t save_string_to_nvs(const char *key, const char *value)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS for key %s with error: %s", key, esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Saving string %s to NVS with key %s", value, key);

    if (err == ESP_OK)
    {
        err = nvs_set_str(nvs_handle, key, value);
        if (err == ESP_OK)
        {
            err = nvs_commit(nvs_handle);
        }
        nvs_close(nvs_handle);
    }
    return err;
}

// Implementation of get functions
esp_err_t get_root_mac(uint8_t *root_mac)
{
    size_t expected_size = 6;
    return get_blob_from_nvs("root_mac", root_mac, &expected_size);
}

esp_err_t get_channel(int *channel)
{
    return get_integer_from_nvs("channel", channel);
}

esp_err_t get_rssi(int *rssi)
{
    return get_integer_from_nvs("rssi", rssi);
}

esp_err_t get_ap_mac(char *ap_mac, size_t max_len)
{
    return get_string_from_nvs("ap_mac", ap_mac, max_len);
}

esp_err_t get_ip_address(char *ip_address, size_t max_len)
{
    return get_string_from_nvs("ip_address", ip_address, max_len);
}

// Generic get functions to reduce code duplication
static esp_err_t get_integer_from_nvs(const char *key, int *value)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS for key %s with error: %s", key, esp_err_to_name(err));
        return err;
    }

    err = nvs_get_i32(nvs_handle, key, value);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Successfully read %s from NVS: %d", key, *value);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to read %s from NVS with error: %s", key, esp_err_to_name(err));
        *value = 0; // You might want to explicitly set *value to 0 in case of failure
    }
    nvs_close(nvs_handle);
    return err;
}

static esp_err_t get_string_from_nvs(const char *key, char *value, size_t max_len)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK)
    {
        err = nvs_get_str(nvs_handle, key, value, &max_len);
        nvs_close(nvs_handle);
    }
    return err;
}

static esp_err_t get_blob_from_nvs(const char *key, void *value, size_t *expected_size)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK)
    {
        err = nvs_get_blob(nvs_handle, key, value, expected_size);
        nvs_close(nvs_handle);
    }
    return err;
}
