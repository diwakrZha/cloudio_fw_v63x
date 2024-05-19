#include "vinit.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_mac.h"

/*App core includes*/
#include "buzzer.h"
#include "ldo_control.h"
#include "device_config.h"


#define TAG "VAIOTA_INIT_CONFIG"

char device_id[DEVICE_ID_LEN + 1] = "";
char shadow_get_topic[TOPIC_MAX_LEN] = "";
char shadow_get_accepted_topic[TOPIC_MAX_LEN] = "";
char shadow_update_delta_topic[TOPIC_MAX_LEN] = "";
char shadow_update_topic[TOPIC_MAX_LEN] = "";
char shadow_payload_update_topic[TOPIC_MAX_LEN] = "";
char shadow_error_update_topic[TOPIC_MAX_LEN] = "";
char shadow_device_info_topic[TOPIC_MAX_LEN] = "";

// Define the LED and BUZ flags
uint8_t LED = 1;  // Default value is OFF
uint8_t BUZ = 1;  // Default value is OFF

// Functions to set/get the LED and BUZ flags
void set_LED(uint8_t status) {
    LED = status;
}

uint8_t get_LED(void) {
    return LED;
}

void set_BUZ(uint8_t status) {
    BUZ = status;
}

uint8_t get_BUZ(void) {
    return BUZ;
}


const char *get_device_id(void)
{
    if (device_id[0] == '\0')
    {
        esp_err_t ret = ESP_OK;
        uint8_t mac[6];

        // Get base MAC address from EFUSE BLK0(default option)
        ret = esp_read_mac(mac, ESP_MAC_EFUSE_FACTORY);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to get base MAC address from EFUSE BLK0. (%s)", esp_err_to_name(ret));
        }
        else
        {
            sprintf(device_id, DEVICE_ID_PREFIX "%02x%02x%02x%02x%02x%02x",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            ESP_LOGI(TAG, "Base MAC Address read from EFUSE BLK0 %s", device_id);
        }
    }
    return device_id;
}

const char* get_project_name(void) {
    const esp_app_desc_t *app_desc = esp_app_get_description();
    return app_desc ? app_desc->project_name : NULL;
}

const char* get_project_version(void) {
    const esp_app_desc_t *app_desc = esp_app_get_description();
    return app_desc ? app_desc->version : NULL;
}

void init_shadow_topics(void) {
    get_device_id();  // Ensure device_id is set
    snprintf(shadow_get_topic, TOPIC_MAX_LEN, "$aws/things/%s/shadow/get", device_id);
    snprintf(shadow_get_accepted_topic, TOPIC_MAX_LEN, "$aws/things/%s/shadow/get/accepted", device_id);
    snprintf(shadow_update_delta_topic, TOPIC_MAX_LEN, "$aws/things/%s/shadow/update/delta", device_id);
    snprintf(shadow_update_topic, TOPIC_MAX_LEN, "$aws/things/%s/shadow/update", device_id);
    snprintf(shadow_payload_update_topic, TOPIC_MAX_LEN, "$aws/things/%s/shadow/name/payload-prod/update", device_id);
    snprintf(shadow_error_update_topic, TOPIC_MAX_LEN, "$aws/things/%s/shadow/name/error-logs/update", device_id);
    snprintf(shadow_device_info_topic, TOPIC_MAX_LEN, "$aws/things/%s/shadow/name/device-info/update", device_id);
}


