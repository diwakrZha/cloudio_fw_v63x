#include "aws_config_handler.h"
#include "nvs.h"
#include "string.h"
#include "esp_log.h"
#include "esp_system.h"
#include "vinit.h"

static const char *TAG = "AWS_CONFIG_HANDLER";

#define EXTRACT_AND_LOG_INT(key, dest, jsonString) _extractAndLogInt(key, (void *)&(dest), sizeof(dest), jsonString)


device_conf_t conf = {
    .publish_interval = DEFAULT_PUBLISH_INTERVAL,
    .facility_id = DEFAULT_FACILITY_ID,
    .led = DEFAULT_LED,
    .buz = DEFAULT_BUZZ,
    .wifi_rst = DEFAULT_WIFI_RESET,
    .dbg_mode = DEFAULT_DBG_MODE,
    .radar_read_interval = DEFAULT_RADAR_READ_INTERVAL,
    .radar_publish_interval = DEFAULT_RADAR_PUBLISH_INTERVAL,
    .sensor_read_interval = DEFAULT_SENSOR_READ_INTERVAL,
    .bdrt = DEFAULT_BDRT,
    .qry_str = DEFAULT_QRY_STR,
};

const char *extractValueForKey(const char *jsonString, const char *key)
{
    // Search for the key in the JSON string
    const char *keyPos = strstr(jsonString, key);
    if (keyPos == NULL)
    {
        return NULL; // Key not found
    }

    // Find the start of the value (after the key and the ':' character)
    const char *valueStart = strchr(keyPos, ':');
    if (valueStart == NULL)
    {
        return NULL; // ':' not found after the key
    }
    valueStart++; // Move past the ':' character

    static char valueBuffer[256]; // Adjust size as needed

    if (*valueStart == '"')
    {
        // Value is a string
        valueStart++; // Move past the opening '"'
        const char *valueEnd = strchr(valueStart, '"');
        if (valueEnd == NULL)
        {
            return NULL; // Closing '"' not found
        }

        // Extract the string value
        size_t valueLength = valueEnd - valueStart;
        strncpy(valueBuffer, valueStart, valueLength);
        valueBuffer[valueLength] = '\0';
    }
    else
    {
        // Value is a number (or another type)
        const char *valueEnd = strchr(valueStart, ',');
        if (valueEnd == NULL)
        {
            valueEnd = strchr(valueStart, '}'); // If it's the last item in the object
            if (valueEnd == NULL)
            {
                return NULL; // No valid end found
            }
        }

        // Extract the numeric value
        size_t valueLength = valueEnd - valueStart;
        strncpy(valueBuffer, valueStart, valueLength);
        valueBuffer[valueLength] = '\0';
    }

    return valueBuffer;
}

bool _extractAndLogInt(const char *key, void *dest, size_t size, const char *jsonString) {
    const char *valueStr = extractValueForKey(jsonString, key);
    if (!valueStr) {
        ESP_LOGW(TAG, "%s key not found or invalid!", key);
        return false;
    }

    uint32_t extractedValue = atoi(valueStr);
    switch (size) {
        case sizeof(uint8_t):
            if (extractedValue <= UINT8_MAX) {
                *(uint8_t *)dest = (uint8_t)extractedValue;
            } else {
                ESP_LOGW(TAG, "Value for key %s exceeds uint8_t range!", key);
                return false;
            }
            break;
        case sizeof(uint16_t):
            if (extractedValue <= UINT16_MAX) {
                *(uint16_t *)dest = (uint16_t)extractedValue;
            } else {
                ESP_LOGW(TAG, "Value for key %s exceeds uint16_t range!", key);
                return false;
            }
            break;
        case sizeof(uint32_t):
            *(uint32_t *)dest = extractedValue;
            break;
        default:
            ESP_LOGW(TAG, "Invalid size provided for key %s!", key);
            return false;
    }
    // ESP_LOGI(TAG, "%s: %u", key, extractedValue);
    return true;
}



bool parseCONFIGjson(const char *jsonString) {
    if (!jsonString) {
        ESP_LOGE(TAG, "Input is NULL!");
        return false;
    }

    bool allKeysFound = true;

    allKeysFound &= EXTRACT_AND_LOG_INT("\"publish_interval\"", conf.publish_interval, jsonString);
    allKeysFound &= EXTRACT_AND_LOG_INT("\"led\"", conf.led, jsonString);
    allKeysFound &= EXTRACT_AND_LOG_INT("\"buz\"", conf.buz, jsonString);
    allKeysFound &= EXTRACT_AND_LOG_INT("\"wifi_rst\"", conf.wifi_rst, jsonString);
    allKeysFound &= EXTRACT_AND_LOG_INT("\"dbg_mode\"", conf.dbg_mode, jsonString);
    allKeysFound &= EXTRACT_AND_LOG_INT("\"bdrt\"", conf.bdrt, jsonString);
    allKeysFound &= EXTRACT_AND_LOG_INT("\"qry_str\"", conf.qry_str, jsonString);
    allKeysFound &= EXTRACT_AND_LOG_INT("\"sensor_read_interval\"", conf.sensor_read_interval, jsonString);
    allKeysFound &= EXTRACT_AND_LOG_INT("\"radar_read_interval\"", conf.radar_read_interval, jsonString);
    allKeysFound &= EXTRACT_AND_LOG_INT("\"radar_publish_interval\"", conf.radar_publish_interval, jsonString);

    const char *facility_id = extractValueForKey(jsonString, "\"facility_id\"");
    if (facility_id) {
        strncpy(conf.facility_id, facility_id, sizeof(conf.facility_id) - 1);
        conf.facility_id[sizeof(conf.facility_id) - 1] = '\0'; // Ensure null-termination
    } else {
        ESP_LOGW(TAG, "Facility ID key not found or invalid!");
        allKeysFound = false;
    }

    if (allKeysFound) {
        ESP_LOGI(TAG, "All keys extracted successfully from JSON string!");
    } else {
        ESP_LOGW(TAG, "Some keys were missing or invalid in the JSON string!");
    }

    return allKeysFound;
}



void displayDeviceConfig(const device_conf_t conf)
{
    ESP_LOGI(TAG, "\n**Current Device Configuration**\n");
    ESP_LOGI(TAG, "Publish Interval: %lu", conf.publish_interval);
    ESP_LOGI(TAG, "LED: %d", conf.led);
    ESP_LOGI(TAG, "BUZZ: %d", conf.buz);
    ESP_LOGI(TAG, "WiFi Reset: %d", conf.wifi_rst);
    ESP_LOGI(TAG, "Debug Mode: %d", conf.dbg_mode);
    ESP_LOGI(TAG, "RS485 Modbus Baudrate: %d", conf.bdrt);
    ESP_LOGI(TAG, "RS485 Modbus Querry String: %s", conf.qry_str);
    ESP_LOGI(TAG, "Sensor Read Interval: %lu", conf.sensor_read_interval);
    ESP_LOGI(TAG, "Radar Read Interval: %lu", conf.radar_read_interval);
    ESP_LOGI(TAG, "Radar Publish Interval: %lu", conf.radar_publish_interval);
    ESP_LOGI(TAG, "Facility ID: %s", conf.facility_id);
}

bool processJsonAndStoreConfig(const char *jsonString)
{
    if (!jsonString)
    {
        ESP_LOGE(TAG, "Input is NULL!");
    }

    // Removed conf parameter from the function call
    if (!parseCONFIGjson(jsonString))
    {
        ESP_LOGW(TAG, "Failed to extract all values from JSON string!");
    }

    ESP_LOGI(TAG, "Values extracted from JSON string successfully!");
    displayDeviceConfig(conf); // Use global conf
    return true;
}