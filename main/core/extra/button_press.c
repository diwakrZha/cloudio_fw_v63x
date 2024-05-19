#include "button_press.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "app_wifi.h"
#include "device_config.h"
#include "inttypes.h"
#include "esp_timer.h"
#include "led.h"
#include "ldo_control.h"
#include "uart_handler.h"

static const char* TAG = "BUTTON_PRESS";
static QueueHandle_t gpio_evt_queue = NULL;

static volatile uint64_t last_interrupt_time = 0;

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint64_t current_time = esp_timer_get_time();
    uint32_t gpio_num = (uint32_t) arg;

    // Ensure debounce time has passed since last interrupt
    if (current_time - last_interrupt_time > DEBOUNCE_TIME_MS * 1000) {
        // Post to queue only if debounce condition is met
        xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
        last_interrupt_time = current_time;
    }
}

static void button_task(void* arg) {
    uint32_t io_num;
    TickType_t button_press_time = 0;
    bool logged_five_seconds = false;  // To ensure we log the 5-second mark only once per press
    bool logged_ten_seconds = false;   // To ensure we log the 10-second mark only once per press

    for (;;) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            if (gpio_get_level(io_num) == 0) {  // Button pressed (active low)
                button_press_time = xTaskGetTickCount();
                logged_five_seconds = false;  // Reset log flags for a new button press
                logged_ten_seconds = false;
                ESP_LOGW(TAG, "Button pressed");
                ldo_on();
                led_blue_on();
                //stop_uart_data_task();
                // Start monitoring duration right after press detected
                while(gpio_get_level(io_num) == 0) {  // Continues until the button is released
                    TickType_t current_duration = xTaskGetTickCount() - button_press_time;
                    if (!logged_ten_seconds && current_duration > pdMS_TO_TICKS(RESET_PRESS_TIME_MS)) {
                        ESP_LOGW(TAG, "Button has been pressed for over 10 seconds.");
                        logged_ten_seconds = true;
                        ldo_on();
                        //led_red_fade();
                        led_red_on();
                    }
                    if (!logged_five_seconds && current_duration > pdMS_TO_TICKS(LONG_PRESS_TIME_MS)) {
                        ESP_LOGW(TAG, "Button has been pressed for over 5 seconds.");
                        logged_five_seconds = true;
                        ldo_on();
                        led_green_on();
                        //led_green_fade();
                    }
                    vTaskDelay(pdMS_TO_TICKS(500)); // Small delay to prevent flooding
                }
                
            } else {  // Button released
                // Now perform actions based on the max duration held before release
                TickType_t press_duration = xTaskGetTickCount() - button_press_time;
                if (logged_ten_seconds) {
                    ESP_LOGW(TAG, "Resetting WiFi credentials...");
                    vTaskDelay(pdMS_TO_TICKS(500));  // Optional delay before taking action
                    reset_wifi_credentials();  // Assume this function exists and properly resets WiFi credentials
                }
                if (logged_five_seconds && !logged_ten_seconds) { // Restart only if not already resetting WiFi
                    ESP_LOGW(TAG, "Restarting board...");
                    vTaskDelay(pdMS_TO_TICKS(500));
                    esp_restart();
                }
                led_off();
                //start_uart_data_task();
            }
        }
    }
}


esp_err_t button_init(void) {
    gpio_config_t io_conf;
    // Configure interrupt for both edges since we need to detect press and release
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = (1ULL << SETTING_BTN9);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;  // Enable pull-up since button is active low
    io_conf.pull_down_en = 0;
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        return ret;
    }

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    if (!gpio_evt_queue) {
        return ESP_FAIL;
    }

    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = gpio_isr_handler_add(SETTING_BTN9, gpio_isr_handler, (void*) SETTING_BTN9);
    if (ret != ESP_OK) {
        return ret;
    }

    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);
    return ESP_OK;
}