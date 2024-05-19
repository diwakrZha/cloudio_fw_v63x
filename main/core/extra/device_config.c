#include "device_config.h"
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"


void set_gpio_for_outputs(const gpio_num_t *gpios, size_t num_gpios)
{
    for (size_t i = 0; i < num_gpios; i++)
    {
        gpio_num_t gpio_num = gpios[i];
        gpio_hold_dis(gpio_num);
        gpio_config_t io_conf = {
            .intr_type = GPIO_INTR_DISABLE,
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = (1ULL << gpio_num),
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .pull_up_en = GPIO_PULLUP_DISABLE};
        gpio_config(&io_conf);
        gpio_set_level(gpio_num, 0);
    }
}

void dis_hold_gpio(const gpio_num_t *gpios, size_t num_gpios)
{
    for (size_t i = 0; i < num_gpios; i++)
    {
        gpio_num_t gpio_num = gpios[i];
        gpio_hold_dis(gpio_num);
    }
}

void en_hold_gpio(const gpio_num_t *gpios, size_t num_gpios)
{
    for (size_t i = 0; i < num_gpios; i++)
    {
        gpio_num_t gpio_num = gpios[i];
        gpio_hold_en(gpio_num);
    }
}


void reset_gpios(const gpio_num_t *gpios, size_t num_gpios)
{
    for (size_t i = 0; i < num_gpios; i++)
    {
        gpio_num_t gpio_num = gpios[i];
        gpio_hold_dis(gpio_num);   // Disable hold
        gpio_reset_pin(gpio_num);  // Reset the pin
    }
}

void set_gpios_to_input_pullup(const gpio_num_t *gpios, size_t num_gpios)
{
    for (size_t i = 0; i < num_gpios; i++)
    {

        gpio_num_t gpio_num = gpios[i];
        gpio_hold_dis(gpio_num);
        gpio_config_t io_conf = {
            .intr_type = GPIO_INTR_DISABLE,
            .mode = GPIO_MODE_INPUT,
            .pin_bit_mask = (1ULL << gpio_num),
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .pull_up_en = GPIO_PULLUP_ENABLE};
        gpio_config(&io_conf);
    }
}

void set_gpios_to_input_pulldown(const gpio_num_t *gpios, size_t num_gpios)
{
    for (size_t i = 0; i < num_gpios; i++)
    {

        gpio_num_t gpio_num = gpios[i];
        gpio_hold_dis(gpio_num);
        gpio_config_t io_conf = {
            .intr_type = GPIO_INTR_DISABLE,
            .mode = GPIO_MODE_INPUT,
            .pin_bit_mask = (1ULL << gpio_num),
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .pull_up_en = GPIO_PULLUP_DISABLE};
        gpio_config(&io_conf);
    }
}

void set_and_hold_gpio_outputs_low_pulldown(const gpio_num_t *gpios, size_t num_gpios)
{
    for (size_t i = 0; i < num_gpios; i++)
    {
        gpio_num_t gpio_num = gpios[i];

        gpio_hold_dis(gpio_num);

        gpio_config_t io_conf = {
            .intr_type = GPIO_INTR_DISABLE,
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = (1ULL << gpio_num),
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .pull_up_en = GPIO_PULLUP_DISABLE};
        gpio_config(&io_conf);
        gpio_set_level(gpio_num, 0);
        gpio_hold_en(gpio_num);
    }
}

void set_and_hold_gpio_outputs_low(const gpio_num_t *gpios, size_t num_gpios)
{
    for (size_t i = 0; i < num_gpios; i++)
    {
        gpio_num_t gpio_num = gpios[i];

        gpio_hold_dis(gpio_num);

        gpio_config_t io_conf = {
            .intr_type = GPIO_INTR_DISABLE,
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = (1ULL << gpio_num),
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .pull_up_en = GPIO_PULLUP_DISABLE};
        gpio_config(&io_conf);
        gpio_set_level(gpio_num, 0);
        gpio_hold_en(gpio_num);
    }
}
