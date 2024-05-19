// led.c
// led.c
#include "led.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"
#include "driver/gpio.h"
#include "ldo_control.h"

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000
#define RMT_LED_STRIP_GPIO_NUM      8
#define FADE_DELAY_MS               20
#define MAX_BRIGHTNESS              227
#define FADE_STEP                   1

static const char *TAG = "LED_DRIVER";
static rmt_channel_handle_t led_chan = NULL;
static rmt_encoder_handle_t led_encoder = NULL;
static bool led_initialized = false;

static void set_color(uint8_t red, uint8_t green, uint8_t blue) {
    ESP_LOGI(TAG, "Setting color: R=%d, G=%d, B=%d", red, green, blue);
    ESP_LOGW(TAG, "LED initialized: %d", led_initialized);
    if (!led_initialized) return; // Guard to prevent usage if not initialized
    ldo_on();
    uint8_t led_strip_pixels[3] = { green, blue, red };
    rmt_transmit_config_t tx_config = { .loop_count = 0 };

    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
}

void led_initialize(void) {
    ESP_LOGI(TAG, "Initializing RMT and LED encoder");
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_XTAL,
        .gpio_num = RMT_LED_STRIP_GPIO_NUM,
        .mem_block_symbols = 64,
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

    ESP_ERROR_CHECK(rmt_enable(led_chan));
    led_initialized = true;
}

void led_deinitialize(void) {
    if (!led_initialized) return;

    //ESP_ERROR_CHECK(rmt_driver_uninstall(led_chan));
    led_chan = NULL;
    led_encoder = NULL;
    led_initialized = false;
    ESP_LOGI(TAG, "LED deinitialized");
}

bool is_led_initialized(void) {
    return led_initialized;
}

void led_off(void) {
    set_color(0, 0, 0); // All off
    ldo_off();
}

// The color and fade functions remain unchanged


void fade_color(uint8_t red, uint8_t green, uint8_t blue) {
        ESP_LOGW(TAG, "LED initialized: %d", led_initialized);
    if (!led_initialized) return; // Guard to prevent usage if not initialized
    ldo_on();
    int intensity = 0;
    int step = FADE_STEP;

    
    while (1) {
        set_color(red * intensity / MAX_BRIGHTNESS,
                  green * intensity / MAX_BRIGHTNESS,
                  blue * intensity / MAX_BRIGHTNESS);
        vTaskDelay(pdMS_TO_TICKS(FADE_DELAY_MS));

        intensity += step;
        if (intensity > MAX_BRIGHTNESS || intensity < 0) {
            step = -step;
            intensity += step;  // Correct overshoot
        }
        if (intensity == 0) {
            break;  // Stop fading after a full cycle
        }
    }
}

void led_red_on(void) {
    set_color(0, 0, 255); // Red
}

void led_green_on(void) {
    set_color(0, 255, 0); // Green
}

void led_blue_on(void) {
    set_color(255, 0, 0); // Blue
}

void led_red_fade(void) {
    fade_color(0, 0, 255); // Fade Red
}

void led_green_fade(void) {
    fade_color(0, 255, 0); // Fade Green
}

void led_blue_fade(void) {
    fade_color(255, 0, 0); // Fade Blue
}
