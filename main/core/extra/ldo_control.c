#include "ldo_control.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "vinit.h"
#include "device_config.h"

void ldo_init()
{
    // Initialization code remains the same
    //gpio_pad_select_gpio(LDO_CONTROL_GPIO);
    gpio_set_direction(LDO_CONTROL_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LDO_CONTROL_GPIO, 0);
}

void ldo_on()
{
    gpio_reset_pin(LDO_CONTROL_GPIO);
    gpio_set_level(LDO_CONTROL_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}


void ldo_off()
{
    gpio_reset_pin(LDO_CONTROL_GPIO);
    gpio_set_level(LDO_CONTROL_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
}

void eth_pwr_on()
{
    gpio_reset_pin(LDO_CONTROL_GPIO);
    gpio_set_level(ETH_POWER_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

void eth_pwr_off()
{
    gpio_reset_pin(LDO_CONTROL_GPIO);
    gpio_set_level(ETH_POWER_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(30));
}