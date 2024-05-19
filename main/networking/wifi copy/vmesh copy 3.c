#include "nvs_flash.h"
#include "esp_netif.h"
#include "zh_espnow.h"
#include "esp_random.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include <time.h>
#include <sys/time.h>

/*App core includes*/
#include "buzzer.h"
#include "ldo_control.h"
#include "led.h"
#include "sensors.h"
#include "ping_time.h"
#include "dev_wdt.h"
#include "vinit.h"
#include "device_config.h"
#include "dev_wdt.h"
#include "psm_routine.h"

#include "vmesh.h"


#define CONFIG_ESPNOW_CHANNEL 1
#define CONFIG_ESPNOW_ENABLE_LONG_RANGE 1
#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF ESP_IF_WIFI_STA
#define TIME_DATE_STR_LEN 30 // Adjust based on your timestamp format
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]

static const char *TAG = "SENSA_MAIN";

void zh_espnow_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

static uint8_t target[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

const int wakeup_time_sec = 10; // Time to sleep before waking up in seconds

// uint8_t target[6] = {0x34, 0x94, 0x54, 0x24, 0xA3, 0x41};
// uint8_t target[6] = {0x34, 0x94, 0x54, 0x24, 0xA3, 0x41};

#define CHAR_VALUE_SIZE 100 // Increase as needed
typedef struct example_message_t
{
    char char_value[CHAR_VALUE_SIZE];
    // int int_value;
    // float float_value;
    // bool bool_value;
} example_message_t;

static void example_deep_sleep_register_rtc_timer_wakeup(void)
{
    printf("Enabling timer wakeup, %ds\n", wakeup_time_sec);
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000));
}

void get_current_timestamp(char *buffer, size_t buffer_len)
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(buffer, buffer_len, "%Y-%m-%d %H:%M:%S", &timeinfo);‚‚
}

// static bool is_time_set()
// {
//     time_t now;
//     time(&now);

//     // Define a threshold time, for example, January 1, 2021 (Unix timestamp)
//     time_t threshold_time = 1609459200; // Adjust this threshold as needed

//     return now > threshold_time;
// }

// static void set_time_using_timestamp()
// {
//     if (!is_time_set())
//     {
//         struct timeval now = {.tv_sec = CURRENT_TIMESTAMP};

//         if (settimeofday(&now, NULL) != 0)
//         {
//             ESP_LOGE(TAG, "Time set failed for %d", CURRENT_TIMESTAMP);
//         }
//         else
//         {
//             ESP_LOGI(TAG, "Time set to %d", CURRENT_TIMESTAMP);
//         }
//     }
//     else
//     {
//         printf("Time is already set and valid.\n");
//     }
// }

void mesh_send_task(void)
{

    //set_time_using_timestamp();

    // nvs_flash_init();
    // esp_netif_init();

    // esp_event_loop_create_default();
    // wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    // esp_wifi_init(&wifi_init_config);
    // esp_wifi_set_mode(WIFI_MODE_STA);
    // esp_wifi_start();

    // ESP_ERROR_CHECK(esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    ESP_LOGI(TAG, "Enable long range mode");
    ESP_ERROR_CHECK(esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));

    zh_espnow_init_config_t zh_espnow_init_config = ZH_ESPNOW_INIT_CONFIG_DEFAULT();
    zh_espnow_init(&zh_espnow_init_config);
    esp_event_handler_register(ZH_ESPNOW, ESP_EVENT_ANY_ID, &zh_espnow_event_handler, NULL);

    example_message_t send_message;

    /*initiate LDO and components*/
    setup_wdt(60); // Set up WDT with a 60-second timeout
    feed_wdt();    // Feed the WDT

    get_device_id(); // Ensure device_id is set
    ESP_LOGI(TAG, "Device ID: %s", device_id);
    ESP_LOGI(TAG, "Project Name: %s, Version: %s \n", get_project_name(), get_project_version());

    led_initialize();
    led_green_on();

    example_deep_sleep_register_rtc_timer_wakeup();
    char timestamp[TIME_DATE_STR_LEN];

    for (;;)
    {
        // Assuming env_data and get_distance_val functions are defined and accessible here

        get_current_timestamp(timestamp, sizeof(timestamp));

        // Format the sensor data into the send_message.char_value
        snprintf(send_message.char_value, sizeof(send_message.char_value),
                 "#%s#", timestamp);

        // send_message.int_value = esp_random();
        // send_message.float_value = 1.234; // You can also replace these with sensor data if needed
        // send_message.bool_value = false;

        // zh_espnow_send(NULL, (uint8_t *)&send_message, sizeof(send_message));
        // vTaskDelay(5000 / portTICK_PERIOD_MS);

        zh_espnow_send(target, (uint8_t *)&send_message, sizeof(send_message));
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        feed_wdt();
    }
}

void zh_espnow_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case ZH_ESPNOW_ON_RECV_EVENT:;
        zh_espnow_event_on_recv_t *recv_data = event_data;
        printf("Message from MAC %02X:%02X:%02X:%02X:%02X:%02X is received. Data lenght %d bytes.\n", MAC2STR(recv_data->mac_addr), recv_data->data_len);
        example_message_t *recv_message = (example_message_t *)recv_data->data;
        printf("Char %s\n", recv_message->char_value);
        // printf("Int %d\n", recv_message->int_value);
        // printf("Float %f\n", recv_message->float_value);
        // printf("Bool %d\n", recv_message->bool_value);
        free(recv_data->data); // Do not delete to avoid memory leaks!
        break;
    case ZH_ESPNOW_ON_SEND_EVENT:;
        zh_espnow_event_on_send_t *send_data = event_data;
        if (send_data->status == ZH_ESPNOW_SEND_SUCCESS)
        {
            //printf("Message to MAC %02X:%02X:%02X:%02X:%02X:%02X sent success.\n", MAC2STR(send_data->mac_addr));
        }
        else
        {
            printf("Message to MAC %02X:%02X:%02X:%02X:%02X:%02X sent fail.\n", MAC2STR(send_data->mac_addr));
        }
    default:
        break;
    }
}