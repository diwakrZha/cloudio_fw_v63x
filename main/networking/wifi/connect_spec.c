
#include "connect_spec.h"
#include "esp_http_client.h"
#include "esp_wifi_types.h"
#include "esp_err.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_log.h"

#define MAX_RETRY_COUNT 3

static const char *TAG = "CONNECT_SPEC";

static const char *ip_services[] = {
    "http://api.ipify.org",
    "http://ifconfig.me/ip",
    "http://ipinfo.io/ip"};

const size_t ip_services_count = sizeof(ip_services) / sizeof(ip_services[0]);

uint8_t wifi_mesh_channel; // +1 for the null terminator

int get_wifi_rssi()
{
    wifi_ap_record_t ap_info;
    esp_wifi_sta_get_ap_info(&ap_info);
    int rssi = ap_info.rssi;
    return rssi;
}

uint8_t get_wifi_connection_channel()
{
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
    {
        ESP_LOGI(TAG, "Connected to SSID:%s, Channel:%d", ap_info.ssid, ap_info.primary);
        wifi_mesh_channel = ap_info.primary;
    }
    else
    {
        ESP_LOGI(TAG, "Not currently connected to any AP");
    }
    return wifi_mesh_channel;
}

// Function to get and print the MAC address of the AP we are connected to
char *get_connected_ap_mac(void)
{
    static char ap_mac_str[18] = {0}; // Buffer to hold the MAC address string
    wifi_ap_record_t ap_info;

    // Attempt to get the AP info
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
    {
        // Convert the MAC address to string
        sprintf(ap_mac_str, "%02X:%02X:%02X:%02X:%02X:%02X",
                ap_info.bssid[0], ap_info.bssid[1], ap_info.bssid[2],
                ap_info.bssid[3], ap_info.bssid[4], ap_info.bssid[5]);

        ESP_LOGI(TAG, "Connected AP MAC: %s", ap_mac_str);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to get connected AP info");
        strcpy(ap_mac_str, "00:00:00:00:00:00"); // Default or error value
    }

    return ap_mac_str;
}

char *fetch_external_ip(void)
{
    esp_http_client_config_t config = {0};
    esp_http_client_handle_t client;
    esp_err_t err;

    char *buffer = malloc(128); // Allocate memory for the IP address string
    if (!buffer)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for IP address buffer.");
        return NULL;
    }
    memset(buffer, 0, 128); // Initialize buffer with zeros

    for (size_t i = 0; i < ip_services_count; i++)
    {
        config.url = ip_services[i];
        client = esp_http_client_init(&config);

        ESP_LOGI(TAG, "Trying %s", ip_services[i]);
        err = esp_http_client_perform(client);

        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lli",
                     esp_http_client_get_status_code(client),
                     esp_http_client_get_content_length(client));

            // Diagnostic: Fetch headers first
            esp_http_client_fetch_headers(client);

            vTaskDelay(pdMS_TO_TICKS(100)); // Small delay before reading

            int read_len = esp_http_client_read(client, buffer, 127);
            if (read_len >= 0)
            {
                buffer[read_len] = '\0'; // Ensure null-termination
                ESP_LOGI(TAG, "Read %d bytes. External IP: %s", read_len, buffer);
                esp_http_client_cleanup(client);
                return buffer;
            }
            else
            {
                ESP_LOGE(TAG, "Failed to read response, read_len = %d", read_len);
            }
        }
        else
        {
            ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        }

        esp_http_client_cleanup(client);
        vTaskDelay(pdMS_TO_TICKS(2000)); // Wait for 2 seconds before retrying
    }

    ESP_LOGE(TAG, "Failed to fetch external IP after trying all services.");
    free(buffer); // Free allocated memory if IP address could not be fetched
    return NULL;
}