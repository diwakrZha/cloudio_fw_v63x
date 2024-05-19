/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once
#include <esp_err.h>
#include <stdbool.h>

extern uint8_t wifi_mesh_channel;  // +1 for the null terminator

/** Types of Proof of Possession */
typedef enum {
    /** Use MAC address to generate PoP */
    POP_TYPE_MAC,
    /** Use random stream generated and stored in fctry partition during claiming process as PoP */
    POP_TYPE_RANDOM
} app_wifi_pop_type_t;

void app_wifi_init();
esp_err_t app_wifi_start(app_wifi_pop_type_t pop_type);
esp_err_t reset_wifi_credentials(void);
esp_err_t app_wifi_connect();
bool app_wifi_is_connected();
uint8_t get_wifi_connection_channel();
void vWaitOnWifiConnected( void );