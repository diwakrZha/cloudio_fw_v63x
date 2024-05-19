#include "dev_sleep.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "aws_config_handler.h"
#include "inttypes.h"
#include "esp_pm.h"
#include "uart_handler.h"
#include "sensors.h"

#define TAG "DEVICE_SLEEP"
// Global variables:
int report_period;
TickType_t xTicksToDelay;

void device_sleep(int sleep_type)
{
    // Calculate the report_period with random variation
    report_period = (conf.publish_interval * 1000) + ((rand() % 2001) - 1000);
    if (report_period < 5000) // Ensure you don't end up with a negative delay
    {
        report_period = 5000;
    }

    xTicksToDelay = pdMS_TO_TICKS(report_period);
    ESP_LOGI(TAG, "Ticks to Delay %lu ", xTicksToDelay);

    // Set up and log sleep information
    switch (sleep_type)
    {
    case 0:
        // Only delay without sleeping
        ESP_LOGI(TAG, "Idling for : %ds...", report_period / 1000);
        IDLE_DELAY(report_period / 1000);
        break;

    case 1:
        esp_sleep_enable_timer_wakeup(report_period * 1000); // setup timer for light sleep
        ESP_LOGI(TAG, "Light sleep for : %ds...", report_period / 1000);
        IDLE_DELAY(10); // Delay for 10 ticks to complete the info log
        // IDLE_DELAY(report_period / 1000);
        esp_light_sleep_start();
        break;

    case 2:
        stop_uart_data_task();
        adc_en(false);
        esp_sleep_enable_timer_wakeup(report_period * 1000); // setup timer for deep sleep
        ESP_LOGI(TAG, "Deep sleep for : %ds...", report_period / 1000);
        IDLE_DELAY(10); // Delay for 10 ticks to complete the info log
        // IDLE_DELAY(report_period / 1000);
        esp_deep_sleep_start();
        break;

    default:
        ESP_LOGI(TAG, "Invalid sleep type!");
        break;
    }
}
