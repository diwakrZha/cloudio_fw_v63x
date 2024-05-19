// mesh_sensor.h
#ifndef MESH_SENSOR_H
#define MESH_SENSOR_H

#include "esp_now.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define MAX_CHANNELS 13
#define UNIQUE_ID "YourUniqueID"
#define PAIRING_MESSAGE "PAIR_REQ"
#define DATA_MESSAGE "DATA_MSG"
#define MAX_ESPNOW_MSG_SIZE 250
#define SENSOR_POLL_INTERVAL 1000
#define AES_BLOCK_SIZE 16
#define MAX_RETRIES 3
#define MAX_SEND_RETRIES 3
#define MAX_SEND_QUEUE_SIZE 10
#define QUEUE_SEND_TIMEOUT 1000

#define DEFAULT_ENCRYPTION_KEY "DefaultKey12345" // 16 chars for AES-128

#ifdef __cplusplus
extern "C" {
#endif

void mesh_send_task();
void stop_mesh_sensor_read();
void start_sensor_data_task();
void stop_sensor_data_task();
void start_send_task();
void stop_send_task();
void deinitialize_sender();

// NVS related functions
esp_err_t save_receiver_info(const uint8_t *mac, int32_t channel);
esp_err_t load_receiver_info(uint8_t *mac, int32_t *channel);


// Encryption functions
esp_err_t encrypt_data_aes(const unsigned char *input, unsigned char *output, const unsigned char *key);
esp_err_t decrypt_data(const unsigned char *input, unsigned char *output, const char *key);

#ifdef __cplusplus
}
#endif

#endif // MESH_SENSOR_H
