#ifndef DEV_SLEEP_H
#define DEV_SLEEP_H

/* FreeRTOS includes. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define IDLE_DELAY(X) vTaskDelay(X * 1000 / portTICK_PERIOD_MS)

// Global variables:
extern int report_period;
extern TickType_t xTicksToDelay;

/**
 * @brief Function to manage device sleep.
 * 
 * @param sleep_type   0: Only delay, 1: Light sleep, 2: Deep sleep
 */
void device_sleep(int sleep_type);

#endif // DEV_SLEEP_H
