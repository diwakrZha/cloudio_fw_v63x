#ifndef VINIT_H
#define VINIT_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#define DEVICE_ID_LEN 18  // "cld-" + 12 characters MAC address.
#define TOPIC_MAX_LEN 256  // Adjust this based on the maximum length of your topics

#define MQTT_ENDPOINT "a3rth2hxbir85k-ats.iot.eu-central-1.amazonaws.com"
#define MQTT_PORT 8883

// Extern declarations for global variables
extern char device_id[DEVICE_ID_LEN + 1];  // +1 for the null terminator
extern char shadow_get_topic[TOPIC_MAX_LEN];
extern char shadow_get_accepted_topic[TOPIC_MAX_LEN];
extern char shadow_update_delta_topic[TOPIC_MAX_LEN];
extern char shadow_update_topic[TOPIC_MAX_LEN];
extern char shadow_payload_update_topic[TOPIC_MAX_LEN];
extern char shadow_error_update_topic[TOPIC_MAX_LEN];
extern char shadow_device_info_topic[TOPIC_MAX_LEN];

// Extern declarations for the LED and BUZ flags
extern uint8_t LED;
extern uint8_t BUZ;

// Function declarations
const char* get_device_id(void);
const char* get_project_name(void);
const char* get_project_version(void);
void init_shadow_topics(void);

// New function declarations for LED and BUZ control
void set_LED(uint8_t status);
uint8_t get_LED(void);
void set_BUZ(uint8_t status);
uint8_t get_BUZ(void);

#define Sleep(X) vTaskDelay(X * 1000 / portTICK_PERIOD_MS);

#endif // VINIT_H
