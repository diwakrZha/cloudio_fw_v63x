#ifndef AWS_CONFIG_HANDLER_H
#define AWS_CONFIG_HANDLER_H

#include "cJSON.h"
#include "nvs.h"
#include "esp_log.h"
#include "stdbool.h"
#include "stdint.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "nvs_platform.h"


#define DEFAULT_PUBLISH_INTERVAL 60
#define DEFAULT_FACILITY_ID "default"
#define DEFAULT_LED 1
#define DEFAULT_BUZZ 1
#define DEFAULT_WIFI_RESET 0
#define DEFAULT_DBG_MODE 1
#define DEFAULT_RADAR_READ_INTERVAL 60
#define DEFAULT_RADAR_PUBLISH_INTERVAL 60
#define DEFAULT_SENSOR_READ_INTERVAL 60
#define DEFAULT_BDRT 38400
#define DEFAULT_QRY_STR "3101valuer\n\r"
#define DEFAULT_QRY_STR_LENGTH 64
#define FACILITY_ID_MAX_LENGTH 128

#define STR "str"
#define U8 "u8"
#define U16 "u16"
#define U32 "u32"
#define READ "read"
#define WRITE "write"

typedef struct {
    uint32_t publish_interval;
    char facility_id[FACILITY_ID_MAX_LENGTH]; // Adjust size as needed
    uint8_t led;
    uint8_t buz;
    uint8_t wifi_rst;
    uint8_t dbg_mode;
    uint32_t radar_read_interval;   
    uint32_t radar_publish_interval;
    uint32_t sensor_read_interval;
    uint16_t bdrt; // Modbus Baudrate
    char qry_str[DEFAULT_QRY_STR_LENGTH]; // Adjust size as needed
} device_conf_t;

extern device_conf_t conf;

const char *extractValueForKey(const char *jsonString, const char *key);
bool parseCONFIGjson(const char *jsonString);
void displayDeviceConfig(const device_conf_t conf);
bool processJsonAndStoreConfig(const char *jsonString);

#endif // AWS_CONFIG_HANDLER_H
