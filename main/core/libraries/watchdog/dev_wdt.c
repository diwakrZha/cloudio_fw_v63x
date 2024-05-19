// dev_wdt.c

#include "dev_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "inttypes.h"

static const char* TAG = "WDT"; // Tag for logging
static esp_timer_handle_t watchdog_timer = NULL;
static int current_wdt_timeout_sec = 0;
static int64_t last_feed_time = 0; // Store the last feed time in microseconds

static void wdt_callback(void* arg) {
    ESP_LOGE(TAG, "*Watchdog Timer Triggered!\n");
    esp_restart();
}

void setup_wdt(int wdt_timeout_sec) {
    if (watchdog_timer && current_wdt_timeout_sec == wdt_timeout_sec) {
        return;
    }
    if (watchdog_timer) {
        esp_timer_stop(watchdog_timer);
        esp_timer_delete(watchdog_timer);
        watchdog_timer = NULL;
    }
    esp_timer_create_args_t wdt_config = {
        .callback = &wdt_callback,
        .name = "watchdog"
    };
    esp_timer_create(&wdt_config, &watchdog_timer);
    esp_timer_start_periodic(watchdog_timer, wdt_timeout_sec * 1000000);
    current_wdt_timeout_sec = wdt_timeout_sec;
    last_feed_time = esp_timer_get_time(); // Initialize last feed time
    ESP_LOGI(TAG, "*WDT set up with %d s timeout\n", wdt_timeout_sec);
}

void feed_wdt() {
    if (watchdog_timer) {
        int64_t current_time = esp_timer_get_time();
        int64_t time_since_last_feed = (current_time - last_feed_time) / 1000000; // Convert to seconds
        ESP_LOGI(TAG, "*Elapsed time %lld s, Feeding WDT \n", time_since_last_feed);
        esp_timer_stop(watchdog_timer);
        esp_timer_start_periodic(watchdog_timer, current_wdt_timeout_sec * 1000000);
        last_feed_time = current_time; // Update last feed time
    }
}

void disable_wdt() {
    if (watchdog_timer) {
        esp_timer_stop(watchdog_timer);
        esp_timer_delete(watchdog_timer);
        watchdog_timer = NULL;
        ESP_LOGI(TAG, "*Watchdog Timer disabled\n");
    }
}
