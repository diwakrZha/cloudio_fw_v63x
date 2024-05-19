#include "json_payload.h"
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include <stdio.h>
#include <ctype.h>
/* ESP-IDF includes. */
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_system.h"

#include "cJSON.h"
#include "sensors.h"
#include "ping_time.h"
#include "uart_handler.h"
#include "aws_config_handler.h"
#include "vinit.h"
#include "reset_reason.h"
#include "app_wifi.h"
#include "connect_spec.h"
#include "nvs_data_ops.h"

// #include "mb_uart_test.h"

/* task configurations include. */
#include "shadow_pub_conf.h"
#include "get_mesh_data.h"
#include "messaging.h"

#define MAX_UART_DATA_LENGTH 127

const static char *TAG = "JSON_PAYLOAD";

static int ap_channel = -1;      // Initialize to an impossible value for debugging
char ap_mac[18], ip_address[16]; // Adjust sizes as needed

char *formatSensorData(const char *jsonData)
{
    // Parse the JSON data
    cJSON *json = cJSON_Parse(jsonData);
    if (json == NULL)
    {
        // Handle error
        return "nan";
    }

    // Extract fields
    int id = cJSON_GetObjectItem(json, "id")->valueint;
    int type = cJSON_GetObjectItem(json, "type")->valueint;
    int ts = cJSON_GetObjectItem(json, "ts")->valueint;
    int channel = cJSON_GetObjectItem(json, "channel")->valueint;
    float distance = cJSON_GetObjectItem(json, "distance")->valuedouble;       // Changed to valuedouble
    float temperature = cJSON_GetObjectItem(json, "temperature")->valuedouble; // Changed to valuedouble
    float humidity = cJSON_GetObjectItem(json, "humidity")->valuedouble;       // Changed to valuedouble
    float pressure = cJSON_GetObjectItem(json, "pressure")->valuedouble;       // Changed to valuedouble
    float gasres = cJSON_GetObjectItem(json, "gasres")->valuedouble;           // Changed to valuedouble

    // Format the string
    char *formattedStr = malloc(256); // Ensure this is large enough for your needs
    if (formattedStr != NULL)
    {
        sprintf(formattedStr, "%d#%d#%d#%d#%f#%f#%f#%f#%f", id, type, ts, channel, distance, temperature, humidity, pressure, gasres);
    }

    // Clean up
    cJSON_Delete(json);

    return formattedStr;
}

char *get_vsense_data()
{
    char *messageString = "nan";
    cJSON *message = getLatestIncomingMessage();
    if (message)
    {
        // Convert the message to a string
        messageString = cJSON_Print(message);
        if (messageString)
        {
            ESP_LOGI(TAG, "Latest message: %s", messageString);
            // Note: We're not freeing messageString here anymore
        }
        cJSON_Delete(message); // Clean up
    }
    else
    {
        ESP_LOGI(TAG, "No latest message available.");
        return "nan";
    }
    return messageString; // Return the JSON string or NULL if not available
}

void insertDotBeforeEnd(char *str)
{
    int length = strlen(str);

    char modifiedString[length + 2 + 1]; // +2 for the dot and +1 for the null terminator

    strncpy(modifiedString, str, length - 2);
    modifiedString[length - 2] = '.';
    strncpy(modifiedString + length - 1, str + length - 2, 3);
    modifiedString[length + 2] = '\0';

    strcpy(str, modifiedString);
}

void extractValues(const char *input, char *phValue, char *conductivityValue, size_t bufferSize)
{
    char *token;
    char *context;

    // Extract the value between the first and second hash (pH value)
    token = strtok_r((char *)input, "#", &context);
    if (token != NULL)
    {
        token = strtok_r(NULL, "#", &context);
        if (token != NULL)
        {
            strncpy(phValue, token, bufferSize - 1);
            phValue[bufferSize - 1] = '\0';
            insertDotBeforeEnd(phValue);
        }
    }

    // Extract the value between the second and third hash (conductivity value)
    if (token != NULL)
    {
        token = strtok_r(NULL, "#", &context);
        if (token != NULL)
        {
            strncpy(conductivityValue, token, bufferSize - 1);
            conductivityValue[bufferSize - 1] = '\0';
        }
    }
}

/**
 * @brief Remove non-alphanumeric characters from a string except for #, /, and \.
 * @param str - The input string.
 * @return The modified string.
 */
char *stringRemoveNonAlphaNum(char *str)
{
    size_t len = strlen(str);
    size_t j = 0;

    for (size_t i = 0; i < len; i++)
    {
        if (isalnum((unsigned char)str[i]) || str[i] == '#' || str[i] == '/' || str[i] == '\\')
        {
            str[j++] = str[i];
        }
    }
    str[j] = '\0';
    return str;
}
void prepareJsonPayload(char *payloadBuf, uint32_t ulValueToNotify, bool *isValidData)
{
    //    temperatureValue = app_driver_temp_sensor_read_celsius();

    static int ap_channel = -1;      // Initialize to an impossible value for debugging
    char ap_mac[18], ip_address[16]; // Adjust sizes as needed

    get_channel(&ap_channel);
    ESP_LOGI(TAG, "AP Channel: %d", ap_channel);
    get_ap_mac(ap_mac, sizeof(ap_mac));
    ESP_LOGI(TAG, "AP MAC: %s", ap_mac);
    get_ip_address(ip_address, sizeof(ip_address));
    ESP_LOGI(TAG, "IP Address: %s", ip_address);

    char phValue[5] = "nan"; // Buffer size should accommodate the expected value length + 1 for null termination
    char conductivityValue[5] = "nan";
    // char cpu_temp[5] = "nan";

    char uart_data[MAX_UART_DATA_LENGTH] = "nan";
    char tmprawdata[MAX_UART_DATA_LENGTH];

    float power_volts = 0;
    int power_raw = 0;

    // uart_initialize();
    // vTaskDelay(50 / portTICK_PERIOD_MS);
    // get_uart_data_loop();
    //  Retrieve data and copy it into uart_data
    const char *valid_data = get_valid_uart_data();
    strncpy(uart_data, valid_data, MAX_UART_DATA_LENGTH - 1);
    uart_data[MAX_UART_DATA_LENGTH - 1] = '\0'; // Ensure null termination

    // uart_data=get_valid_uart_data();
    // get_uart_data(uart_data, MAX_UART_DATA_LENGTH);
    // vTaskDelay(50 / portTICK_PERIOD_MS);
    // uart_remove_driver();

    *isValidData = strcmp(valid_data, "nan") != 0; // Set flag based on UART data validity

    // Modify the input string by removing non-alphanumeric characters and # and \ and /
    stringRemoveNonAlphaNum(uart_data);
    ESP_LOGI(TAG, "Filtered UART data: %s \n", uart_data);

    strcpy(tmprawdata, uart_data); // Copy data to rxdata
    extractValues(tmprawdata, phValue, conductivityValue, sizeof(phValue));
    ESP_LOGI(TAG, "Got pH value: %s, Conductivity vaue: %s. \n", phValue, conductivityValue);

    cJSON *root, *reported, *report;
    root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "state", reported = cJSON_CreateObject());
    cJSON_AddItemToObject(reported, "reported", report = cJSON_CreateObject());
    char *formattedVSenseData = "nan";
    // char *formattedVSenseData = formatSensorData(get_vsense_data());

    // char *packed_data = get_packed_data();
    // if (packed_data) {
    //     printf("Packed data: %s\n", packed_data);
    //     free(packed_data); // Free the dynamically allocated memory
    // } else {
    //     printf("No data available.\n");
    // }

    char *packed_data = "101#115";

    // Get power voltage and raw ADC value
    get_power_volts(&power_volts, &power_raw);

    // Do something with voltage and raw values
    ESP_LOGI(TAG, "Calibrated Voltage: %f mV", power_volts);
    ESP_LOGI(TAG, "Raw ADC Value: %d", power_raw);

    if (root && reported && report)
    {
        cJSON_AddItemToObject(report, "ts", cJSON_CreateNumber(GetStandardTime()));
        cJSON_AddItemToObject(report, "app_ver", cJSON_CreateString(get_project_name()));
        cJSON_AddItemToObject(report, "device_id", cJSON_CreateString(device_id));
        cJSON_AddItemToObject(report, "facility_id", cJSON_CreateString(conf.facility_id));
        cJSON_AddItemToObject(report, "data_packet_nr", cJSON_CreateNumber(ulValueToNotify));
        cJSON_AddItemToObject(report, "ph", cJSON_CreateString(phValue));
        cJSON_AddItemToObject(report, "conductivity", cJSON_CreateString(conductivityValue));
        cJSON_AddItemToObject(report, "temperature_board", cJSON_CreateString(get_board_temp())); // setting to nan for now because board is giving low temperature consistently
        // cJSON_AddItemToObject(report, "temperature_board", cJSON_CreateString("nan"));

        cJSON_AddItemToObject(report, "rawdat", cJSON_CreateString(uart_data)); // packed data
        cJSON_AddItemToObject(report, "raw_vsens", cJSON_CreateString(formattedVSenseData));

        cJSON_AddItemToObject(report, "wifich", cJSON_CreateNumber(ap_channel));
        cJSON_AddItemToObject(report, "power_volts", cJSON_CreateNumber(power_volts));
        cJSON_AddItemToObject(report, "power_raw", cJSON_CreateNumber(power_raw));

        cJSON_AddItemToObject(report, "reset_reasons", cJSON_CreateNumber(esp_reset_reason()));
        // formatting connection data to resolve location
        cJSON_AddItemToObject(report, "Timestamp", cJSON_CreateNumber(GetStandardTime()));

        //  WiFiAccessPoints Array
        cJSON *wifiAccessPoints = cJSON_AddArrayToObject(report, "WiFiAccessPoints");
        cJSON *wifiAccessPoint = cJSON_CreateObject();
        cJSON_AddItemToObject(wifiAccessPoint, "MacAddress", cJSON_CreateString(ap_mac)); // Fetch the MAC address
        cJSON_AddItemToObject(wifiAccessPoint, "Rss", cJSON_CreateNumber(get_wifi_rssi()));
        cJSON_AddItemToArray(wifiAccessPoints, wifiAccessPoint);

        // Ip Object
        cJSON *ip = cJSON_AddObjectToObject(report, "Ip");
        cJSON_AddItemToObject(ip, "IpAddress", cJSON_CreateString(ip_address));

        cJSON_AddItemToObject(report, "reset_reasons", cJSON_CreateNumber(esp_reset_reason()));
        // free(formattedVSenseData);
        char *jsonString = cJSON_Print(root);
        if (jsonString)
        {
            strncpy(payloadBuf, jsonString, PAYLOAD_STRING_BUFFER_LENGTH - 1);
            payloadBuf[PAYLOAD_STRING_BUFFER_LENGTH - 1] = '\0';
            free(jsonString);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to convert PAYLOAD JSON to string.");
        }
        cJSON_Delete(root);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to create PAYLOAD JSON objects.");
    }
}

void prepReportJsonPayload(char *payloadBuf, uint32_t ulValueToNotify)
{
    cJSON *root, *reported, *reportcfg, *report;
    root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "state", reported = cJSON_CreateObject());
    cJSON_AddItemToObject(reported, "reported", reportcfg = cJSON_CreateObject());
    cJSON_AddItemToObject(reportcfg, "cfg", report = cJSON_CreateObject());

    if (root && reported && reportcfg && report)
    {
        cJSON_AddItemToObject(report, "facility_id", cJSON_CreateString(conf.facility_id));
        cJSON_AddItemToObject(report, "publish_interval", cJSON_CreateNumber(conf.publish_interval));
        cJSON_AddItemToObject(report, "led", cJSON_CreateNumber(conf.led));
        cJSON_AddItemToObject(report, "buz", cJSON_CreateNumber(conf.buz));
        cJSON_AddItemToObject(report, "wifi_rst", cJSON_CreateNumber(0)); // This is to reset the wifi_rst flag in the shadow
        cJSON_AddItemToObject(report, "dbg_mode", cJSON_CreateNumber(conf.dbg_mode));
        cJSON_AddItemToObject(report, "bdrt", cJSON_CreateNumber(conf.bdrt));
        cJSON_AddItemToObject(report, "qry_str", cJSON_CreateString(conf.qry_str));
        cJSON_AddItemToObject(report, "sensor_read_interval", cJSON_CreateNumber(conf.sensor_read_interval));
        cJSON_AddItemToObject(report, "radar_read_interval", cJSON_CreateNumber(conf.radar_read_interval));
        cJSON_AddItemToObject(report, "radar_publish_interval", cJSON_CreateNumber(conf.radar_publish_interval));
        char *jsonString = cJSON_Print(root);
        if (jsonString)
        {
            strncpy(payloadBuf, jsonString, PAYLOAD_STRING_BUFFER_LENGTH - 1);
            payloadBuf[PAYLOAD_STRING_BUFFER_LENGTH - 1] = '\0';
            free(jsonString);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to convert REPORT JSON to string.");
        }
        cJSON_Delete(root);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to create REPORT JSON objects.");
    }
}

void wifi_rst_reinit_JsonPayload(char *payloadBuf, uint32_t ulValueToNotify)
{
    cJSON *root, *reported, *reportcfg, *report;
    root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "state", reported = cJSON_CreateObject());
    cJSON_AddItemToObject(reported, "desired", reportcfg = cJSON_CreateObject());
    cJSON_AddItemToObject(reportcfg, "cfg", report = cJSON_CreateObject());

    if (root && reported && reportcfg && report)
    {
        cJSON_AddItemToObject(report, "wifi_rst", cJSON_CreateNumber(0)); // This is to reset the wifi_rst flag in the shadow
        char *jsonString = cJSON_Print(root);
        if (jsonString)
        {
            strncpy(payloadBuf, jsonString, PAYLOAD_STRING_BUFFER_LENGTH - 1);
            payloadBuf[PAYLOAD_STRING_BUFFER_LENGTH - 1] = '\0';
            free(jsonString);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to convert REPORT JSON to string.");
        }
        cJSON_Delete(root);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to create REPORT JSON objects.");
    }
}