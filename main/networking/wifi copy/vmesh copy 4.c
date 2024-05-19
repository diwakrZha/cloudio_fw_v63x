#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "string.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"

#include "mbedtls/aes.h"

#include "vmesh.h"

// #define MAX_CHANNELS 13
// #define UNIQUE_ID "YourUniqueID"

// #define PAIRING_MESSAGE "PAIR_REQ"
// #define DATA_MESSAGE "DATA_MSG:"
// #define MAX_ESPNOW_MSG_SIZE 250
// #define SENSOR_POLL_INTERVAL 1000 // in milliseconds

// #define AES_BLOCK_SIZE 16
// #define DEFAULT_ENCRYPTION_KEY "DefaultKey123456" // 16 chars for AES-128

static const char *TAG = "VMESH";

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t receiver_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
//uint8_t receiver_mac[ESP_NOW_ETH_ALEN] = {0};

// Global control variable
TaskHandle_t send_task_handle = NULL;
TaskHandle_t sensor_data_task_handle = NULL;
volatile bool send_task_running = false;
volatile bool sensor_task_running = false;
volatile bool is_paired = false;
volatile bool is_enqueueing_data = false;

SemaphoreHandle_t send_queue_semaphore;
QueueHandle_t send_queue;

const char *get_encryption_key()
{
    // Load the key from your configuration source
    // For example, return an environment variable, or return a default key
    return DEFAULT_ENCRYPTION_KEY;
}

void generate_key_from_mac(const uint8_t *mac, const char *base_key, char *final_key, size_t final_key_size)
{
    snprintf(final_key, final_key_size, "%s_%02x%02x%02x%02x%02x%02x",
             base_key, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

esp_err_t encrypt_data(const unsigned char *input, unsigned char *output, const char *key)
{
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_enc(&aes, (const unsigned char *)key, 128) != 0)
    {
        mbedtls_aes_free(&aes);
        return ESP_FAIL;
    }
    // Assume input size is AES_BLOCK_SIZE
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, input, output);
    mbedtls_aes_free(&aes);
    return ESP_OK;
}

esp_err_t decrypt_data(const unsigned char *input, unsigned char *output, const char *key)
{
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_dec(&aes, (const unsigned char *)key, 128) != 0)
    {
        mbedtls_aes_free(&aes);
        return ESP_FAIL;
    }
    // Assume input size is AES_BLOCK_SIZE
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, input, output);
    mbedtls_aes_free(&aes);
    return ESP_OK;
}

esp_err_t save_receiver_info(const uint8_t *mac, int32_t channel)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE("NVS", "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_blob(nvs_handle, "receiver_mac", mac, ESP_NOW_ETH_ALEN);
    if (ret != ESP_OK)
    {
        ESP_LOGE("NVS", "Failed to save MAC: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_set_i32(nvs_handle, "receiver_channel", channel);
    if (ret != ESP_OK)
    {
        ESP_LOGE("NVS", "Failed to save channel: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE("NVS", "Failed to commit NVS: %s", esp_err_to_name(ret));
    }

    nvs_close(nvs_handle);
    return ret;
}

esp_err_t load_receiver_info(uint8_t *mac, int32_t *channel)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE("NVS", "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    size_t required_size = ESP_NOW_ETH_ALEN;
    ret = nvs_get_blob(nvs_handle, "receiver_mac", mac, &required_size);
    if (ret != ESP_OK)
    {
        ESP_LOGE("NVS", "Failed to read MAC: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_get_i32(nvs_handle, "receiver_channel", channel);
    if (ret != ESP_OK)
    {
        ESP_LOGE("NVS", "Failed to read channel: %s", esp_err_to_name(ret));
    }

    nvs_close(nvs_handle);
    return ret;
}

esp_err_t encrypt_data_aes(const unsigned char *input, unsigned char *output, const unsigned char *key)
{
    mbedtls_aes_context aes;
    unsigned char input_padded[AES_BLOCK_SIZE];
    size_t input_len = strlen((const char *)input);

    // Check if input length is not more than AES_BLOCK_SIZE
    if (input_len > AES_BLOCK_SIZE)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Clear and then copy input to padded input buffer
    memset(input_padded, 0, AES_BLOCK_SIZE);
    memcpy(input_padded, input, input_len);

    // Initialize AES context and set encryption key
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_enc(&aes, key, 128) != 0)
    {
        mbedtls_aes_free(&aes);
        return ESP_FAIL;
    }

    // Perform AES encryption
    if (mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, input_padded, output) != 0)
    {
        mbedtls_aes_free(&aes);
        return ESP_FAIL;
    }

    mbedtls_aes_free(&aes);
    return ESP_OK;
}

void enqueue_data_for_sending(const char *data)
{
    is_enqueueing_data = true;
    if (send_queue != NULL)
    {
        // Acquire the semaphore before accessing the queue
        if (xSemaphoreTake(send_queue_semaphore, QUEUE_SEND_TIMEOUT) == pdTRUE)
        {
            if (uxQueueSpacesAvailable(send_queue) == 0)
            {
                // Queue is full, remove the oldest item
                char temp_data[MAX_ESPNOW_MSG_SIZE];
                xQueueReceive(send_queue, &temp_data, 0); // Non-blocking dequeue
                ESP_LOGW("enqueue_data_for_sending", "Queue full, oldest data removed");
            }

            if (xQueueSend(send_queue, data, QUEUE_SEND_TIMEOUT) != pdTRUE)
            {
                ESP_LOGE("enqueue_data_for_sending", "Failed to enqueue data");
            }

            // Release the semaphore after queue operation is done
            xSemaphoreGive(send_queue_semaphore);
        }
        else
        {
            ESP_LOGE("enqueue_data_for_sending", "Semaphore take failed");
        }
    }
    is_enqueueing_data = false;
}

// Function to get sensor data
char *get_sensor_data()
{
    static char sensor_data[MAX_ESPNOW_MSG_SIZE];
    // Poll your sensor and fill sensor_data
    snprintf(sensor_data, sizeof(sensor_data), "Data: %d", 50);
    return sensor_data;
}
void sensor_data_task(void *pvParameter)
{
    while (sensor_task_running)
    {
        char *data = get_sensor_data(); // Make sure this is a non-blocking function
        enqueue_data_for_sending(data);
        vTaskDelay(pdMS_TO_TICKS(SENSOR_POLL_INTERVAL));
    }
    sensor_data_task_handle = NULL;
    vTaskDelete(NULL);
}

void start_sensor_data_task()
{
    if (sensor_data_task_handle == NULL)
    {
        sensor_task_running = true;
        xTaskCreate(sensor_data_task, "sensor_data_task", 2048, NULL, 10, &sensor_data_task_handle);
    }
}

void stop_sensor_data_task()
{
    if (sensor_data_task_handle != NULL)
    {
        sensor_task_running = false;
        vTaskDelete(sensor_data_task_handle);
        sensor_data_task_handle = NULL;
    }
}

void send_data_task(void *pvParameter)
{
    char data_to_send[MAX_ESPNOW_MSG_SIZE];
    while (1)
    {
        if (xQueueReceive(send_queue, &data_to_send, portMAX_DELAY))
        {
            //char encrypted_data[MAX_ESPNOW_MSG_SIZE];
            //ESP_LOGW(TAG, "Encrypting data: %s", data_to_send);
            //encrypt_data_aes((const unsigned char *)data_to_send, (unsigned char *)encrypted_data, (const unsigned char *)DEFAULT_ENCRYPTION_KEY);
            ESP_LOGW(TAG, "Send data task, data: %s", data_to_send);
            //ESP_LOGW(TAG, "Encrypted Send data size, data: %d", strlen(encrypted_data));
            //ESP_LOGW(TAG, "Encrypted Send data, data: %s", encrypted_data);

            //esp_now_send(receiver_mac, (uint8_t *)encrypted_data, strlen(encrypted_data));
            esp_now_send(receiver_mac, (uint8_t *)data_to_send, strlen(data_to_send));
        }
        else
        {
            ESP_LOGE(TAG,"send data task Failed");
            break; // Exit the loop if the queue is stopped
        }
    }
    send_task_handle = NULL;
    vTaskDelete(NULL);
}

void start_send_task()
{
    if (send_task_handle == NULL)
    {
        xTaskCreate(send_data_task, "send_data_task", 4096, NULL, 10, &send_task_handle);
    }
}

void stop_send_task()
{
    if (send_task_handle != NULL)
    {
        // Wait until no data is being enqueued
        while (is_enqueueing_data)
        {
            vTaskDelay(pdMS_TO_TICKS(10)); // Wait for enqueueing to finish
        }

        if (xSemaphoreTake(send_queue_semaphore, QUEUE_SEND_TIMEOUT) == pdTRUE)
        {
            vTaskDelete(send_task_handle);
            send_task_handle = NULL;
            xSemaphoreGive(send_queue_semaphore);
        }
        else
        {
            ESP_LOGE("stop_send_task", "Semaphore take failed");
        }
    }
}

void espnow_send_callback(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS)
    {
        printf("Send Success to MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
               mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    }
}

void espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    // Check if the message is a pairing response
    // This is a simplified check; you might need a more robust validation
    if (strcmp((char *)data, "PAIR_RESP") == 0)
    {
        memcpy(receiver_mac, mac_addr, ESP_NOW_ETH_ALEN);
        is_paired = true;
        ESP_LOGI("espnow_recv_cb", "Paired with receiver %s", receiver_mac);
    }
}
void pair_with_receiver()
{
    uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    char pairing_request[MAX_ESPNOW_MSG_SIZE];
    snprintf(pairing_request, sizeof(pairing_request), "%s:%s", PAIRING_MESSAGE, UNIQUE_ID);
    //char encrypted_request[MAX_ESPNOW_MSG_SIZE];
    //encrypt_data_aes((const unsigned char *)pairing_request,
    //                 (unsigned char *)encrypted_request,
    //                 (const unsigned char *)DEFAULT_ENCRYPTION_KEY);

    int channel_found = NULL;

    for (int i = 1; i <= MAX_CHANNELS; i++)
    {
        //esp_wifi_set_channel(i, WIFI_SECOND_CHAN_NONE);
        //vTaskDelay(pdMS_TO_TICKS(500)); // Delay for response
        //esp_now_send(broadcast_mac, (uint8_t *)encrypted_request, strlen(encrypted_request));
        ESP_LOGW(TAG, "Sending pairing request: %s on channel, %d", pairing_request, i);
        esp_now_send(broadcast_mac, (uint8_t *)pairing_request, strlen(pairing_request));
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for response
        
        if (is_paired)
        {
            channel_found = i;
            break; // Exit the loop if receiver is found
        }
    }

    if (channel_found != NULL)
    {
        // Add receiver to ESP-NOW peer list with encryption

        esp_now_peer_info_t peer_info = {};
        memcpy(peer_info.peer_addr, receiver_mac, ESP_NOW_ETH_ALEN);
        peer_info.channel = channel_found; // Use the channel on which receiver was found
        peer_info.encrypt = true;
        memcpy(peer_info.lmk, DEFAULT_ENCRYPTION_KEY, ESP_NOW_KEY_LEN);

        esp_err_t add_status = esp_now_add_peer(&peer_info);
        if (add_status != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to add peer: %s", esp_err_to_name(add_status));
        }
        else
        {
            ESP_LOGW(TAG, "Added peer successfully on channel %d", channel_found);
            save_receiver_info(receiver_mac, channel_found);
        }
    }
}

void deinitialize_sender()
{
    // Stop and delete sensor data task
    if (sensor_data_task_handle != NULL)
    {
        sensor_task_running = false;
        vTaskDelete(sensor_data_task_handle);
        sensor_data_task_handle = NULL;
    }

    // Stop and delete send data task
    if (send_task_handle != NULL)
    {
        send_task_running = false;
        vTaskDelete(send_task_handle);
        send_task_handle = NULL;
    }

    // Delete the semaphore
    if (send_queue_semaphore != NULL)
    {
        vSemaphoreDelete(send_queue_semaphore);
        send_queue_semaphore = NULL;
    }

    // Delete the queue
    if (send_queue != NULL)
    {
        vQueueDelete(send_queue);
        send_queue = NULL;
    }

    // Deinitialize ESP-NOW
    esp_now_deinit();

    // Deinitialize NVS
    // nvs_flash_deinit();

    // Add here any other resource deinitialization if needed
}

void mesh_send_task()
{
    // Initialize NVS, Wi-Fi, ESP-NOW

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK)
    {
        printf("Error initializing ESP-NOW\n");
        return;
    }

    send_queue_semaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(send_queue_semaphore); // Initially release the semaphore

    esp_now_register_recv_cb(espnow_recv_cb);

    // Create a queue for sending data
    send_queue = xQueueCreate(10, MAX_ESPNOW_MSG_SIZE);
    // Register ESP-NOW send callback
    esp_now_register_send_cb(espnow_send_callback);

    // Pair with receiver
    pair_with_receiver();

    // xTaskCreate(sensor_data_task, "sensor_data_task", 2048, NULL, 10, NULL);

    start_sensor_data_task();
    start_send_task();
    // Create a task for sending data
    // xTaskCreate(send_data_task, "send_data_task", 2048, NULL, 10, NULL);
}
