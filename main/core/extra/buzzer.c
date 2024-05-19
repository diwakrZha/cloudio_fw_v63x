#include "buzzer.h"
#include "vinit.h"           // Include the vinit.h to access the get_BUZ() function
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_log.h>
#include "device_config.h"

#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_TIMER LEDC_TIMER_0
#define DUTY_CYCLE (2048)


const static char *TAG = "BUZZER";

static void set_frequency_and_duty(int freq, int duty) {
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER, freq);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);
}

static void set_duty_zero() {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);
}

static esp_err_t bz_init(void) {

    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_12_BIT,
        .freq_hz = 2000,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .channel = LEDC_CHANNEL,
        .duty = DUTY_CYCLE,
        .gpio_num = BUZZER_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint = 0,
        .timer_sel = LEDC_TIMER,
    };
    ledc_channel_config(&ledc_channel);

    return ESP_OK;
}

static void bz_stop() {
    bz_init();
    set_duty_zero();
    ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, 0);
}

static void bz_fadeout() {
    bz_init();
    int current_duty = ledc_get_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);
    while (current_duty > 0) {
        current_duty -= 10;
        if (current_duty < 0) current_duty = 0;
        set_frequency_and_duty(ledc_get_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER), current_duty);
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
    bz_stop();
}

static void play_beat(int lub_freq, int dub_freq) {
    if (get_BUZ() != 1) return; // Check for BUZ condition
    bz_init();
    set_frequency_and_duty(lub_freq, DUTY_CYCLE);
    vTaskDelay(80 / portTICK_PERIOD_MS);
    set_duty_zero();
    vTaskDelay(40 / portTICK_PERIOD_MS);
    set_frequency_and_duty(dub_freq, DUTY_CYCLE);
    vTaskDelay(60 / portTICK_PERIOD_MS);
    set_duty_zero();
    vTaskDelay(700 / portTICK_PERIOD_MS);
    bz_fadeout();
}

void bz_chirp_up() {
    if (get_BUZ() != 1) return; // Check for BUZ condition
    ESP_LOGI(TAG, "Playing chirp up");
    bz_init();
    for (int freq = 100; freq < 1500; freq += 100) {
        set_frequency_and_duty(freq, DUTY_CYCLE);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    bz_fadeout();
}

void bz_chirp_down() {
    if (get_BUZ() != 1) return; // Check for BUZ condition
    ESP_LOGI(TAG, "Playing chirp down");
    bz_init();
     for (int freq = 1500; freq > 100; freq -= 200) {
        set_frequency_and_duty(freq, DUTY_CYCLE);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    bz_fadeout();
}

void bz_beat_down() {
    if (get_BUZ() != 1) return; // Check for BUZ condition
    ESP_LOGI(TAG, "Playing beat down");
    bz_init();
    play_beat(1500, 500);
}

void bz_beat_up() {
    if (get_BUZ() != 1) return; // Check for BUZ condition
    ESP_LOGI(TAG, "Playing beat up");
    bz_init();
    play_beat(500, 1500);
}

void disable_bz() {
    bz_stop();
    gpio_set_level(BUZZER_GPIO, 0);
    gpio_set_direction(BUZZER_GPIO, GPIO_MODE_DISABLE);
}

void buzzer_task(void *pvParameter) {
    void (*buzzer_function)() = pvParameter;
    buzzer_function(); // Call the buzzer function
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    vTaskDelete(NULL); // Delete the task after execution
}

void start_buzzer_task(void (*buzzer_function)()) {
    xTaskCreate(buzzer_task, "Buzzer Task", 2048, buzzer_function,5, NULL);
}
