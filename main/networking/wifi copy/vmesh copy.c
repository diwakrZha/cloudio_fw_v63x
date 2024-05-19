#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_wifi.h"

#include "vmesh.h"
#include "zh_network.h"

static const char *TAG = "V_MESH";

// ESPNOW settings
#define TIME_DATE_STR_LEN 30 // Adjust based on your timestamp format
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define CHAR_VALUE_SIZE 100 // Increase as needed

#define CONFIG_ESPNOW_ENABLE_LONG_RANGE 1
#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF ESP_IF_WIFI_STA

static TaskHandle_t vmesh_task_handle = NULL;
void zh_network_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

const int wakeup_time_sec = 10; // Time to sleep before waking up in seconds
char timestamp[TIME_DATE_STR_LEN];

volatile bool vmesh_in_use = false; // Global flag

typedef struct example_message_t
{
    char char_value[CHAR_VALUE_SIZE];
    // int int_value;
    // float float_value;
    // bool bool_value;
} example_message_t;

example_message_t send_message;

void get_current_timestamp(char *buffer, size_t buffer_len)
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(buffer, buffer_len, "%Y-%m-%d %H:%M:%S", &timeinfo);
}

void mesh_send_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Channel:%d", wifi_mesh_channel);

    // ESP_ERROR_CHECK(esp_wifi_set_channel(wifi_mesh_channel, WIFI_SECOND_CHAN_NONE));
    ESP_LOGI(TAG, "Enable long range mode");
    ESP_ERROR_CHECK(esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));

    zh_network_init_config_t zh_network_init_config = ZH_NETWORK_INIT_CONFIG_DEFAULT();
    zh_network_init_config.id_vector_size = 150; // Just for an example of how to change the default values.
    zh_network_init(&zh_network_init_config);
    esp_event_handler_instance_register(ZH_NETWORK, ESP_EVENT_ANY_ID, &zh_network_event_handler, NULL, NULL);

    while (true)
    {
        vmesh_in_use = true;
        get_current_timestamp(timestamp, sizeof(timestamp));

        // Format the sensor data into the send_message.char_value
        snprintf(send_message.char_value, sizeof(send_message.char_value),
                 "#%s#", timestamp);

        zh_network_send(NULL, (uint8_t *)&send_message, sizeof(send_message));
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        vmesh_in_use = false;
    }
}
void zh_network_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case ZH_NETWORK_ON_RECV_EVENT:;
        zh_network_event_on_recv_t *recv_data = event_data;
        printf("Message from MAC %02X:%02X:%02X:%02X:%02X:%02X is received. Data lenght %d bytes.\n", MAC2STR(recv_data->mac_addr), recv_data->data_len);
        example_message_t *recv_message = (example_message_t *)recv_data->data;
        printf("Char %s\n", recv_message->char_value);
        // printf("Int %d\n", recv_message->int_value);
        // printf("Float %f\n", recv_message->float_value);
        // printf("Bool %d\n", recv_message->bool_value);
        free(recv_data->data); // Do not delete to avoid memory leaks!
        break;
    case ZH_NETWORK_ON_SEND_EVENT:;
        zh_network_event_on_send_t *send_data = event_data;
        if (send_data->status == ZH_NETWORK_SEND_SUCCESS)
        {
            printf("Message to MAC %02X:%02X:%02X:%02X:%02X:%02X sent success.\n", MAC2STR(send_data->mac_addr));
        }
        else
        {
            printf("Message to MAC %02X:%02X:%02X:%02X:%02X:%02X sent fail.\n", MAC2STR(send_data->mac_addr));
        }
        break;
    default:
        break;
    }
}

void vmesh_stop(void)
{

    ESP_LOGI(TAG, "Stoping vmesh task");
    while (vmesh_in_use)
    {
        // Wait for the sensor operation to complete
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (vmesh_task_handle != NULL)
    {
        zh_network_deinit();
        vTaskDelete(vmesh_task_handle);
        vmesh_task_handle = NULL;
    }
}

void vmesh_init()
{
            
    //ESP_LOGI(TAG, "WIFI channel %d", wifi_mesh_channel)

    ESP_ERROR_CHECK(esp_wifi_set_channel(wifi_mesh_channel, WIFI_SECOND_CHAN_NONE));
    ESP_LOGI(TAG, "Enable long range mode");
    ESP_ERROR_CHECK(esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));

    zh_network_init_config_t zh_network_init_config = ZH_NETWORK_INIT_CONFIG_DEFAULT();
    zh_network_init_config.id_vector_size = 150; // Just for an example of how to change the default values.
    zh_network_init(&zh_network_init_config);
    esp_event_handler_instance_register(ZH_NETWORK, ESP_EVENT_ANY_ID, &zh_network_event_handler, NULL, NULL);

    // Initialize the send_message
    // memset(&send_message, 0, sizeof(example_message_t));
    // snprintf(send_message.char_value, sizeof(send_message.char_value), "Hello World!");
    // send_message.int_value = 123;
    // send_message.float_value = 123.456;
    // send_message.bool_value = true;

    // Start the mesh send task
    xTaskCreate(mesh_send_task, "mesh_send_task", 2048, NULL, 5, &vmesh_task_handle);
}


