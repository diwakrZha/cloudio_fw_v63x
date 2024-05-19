#include "psm_routine.h"
#include "esp_pm.h"
#include "esp_wifi.h"
#include "esp_sleep.h"
#include <time.h>
#include "driver/rtc_io.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c.h"

#include "vinit.h"
#include "led.h"
#include "dev_sleep.h"
#include "ldo_control.h"
#include "device_config.h"
#include "uart_handler.h"
#include "sensors.h"
#include <esp_wifi.h>


#define CONFIG_PM_ENABLE 1
#define CONFIG_FREERTOS_USE_TICKLESS_IDLE 1
#define DEFAULT_PS_MODE WIFI_PS_MAX_MODEM
// #define DEFAULT_PS_MODE WIFI_PS_NONE

static const char *TAG = "PSM_ROUTINE";

// OUTPUT GPIOS
const gpio_num_t gpios_output[] = {
    // BUZZER_GPIO,
    LDO_CONTROL_GPIO,
    ETH_POWER_GPIO,
    LED_STRIP_BLINK_GPIO,
    // BATT_SENSE_EN_GPIO,
    // BATT_SENSE_GPIO,
    // RS485_TXD,
    // RS485_RXD,
    // RS485_CTS
    // LED_STRIP_BLINK_GPIO    //- causes 200 uA more consumption. So, not included
};

const size_t num_gpios_output = sizeof(gpios_output) / sizeof(gpios_output[0]);

// GPIOS to reset
const gpio_num_t gpios_to_reset[] = {
    LED_STRIP_BLINK_GPIO,
    LDO_CONTROL_GPIO,
    ETH_POWER_GPIO,
    EXT_SPI_CS1_GPIO,
};

const size_t num_gpios_to_reset = sizeof(gpios_to_reset) / sizeof(gpios_to_reset[0]);

// SPI lines sleep
const gpio_num_t spi_lines[] = {
    ETH_SPI_SCLK_GPIO,
    ETH_SPI_MOSI_GPIO,
    ETH_SPI_MISO_GPIO,
    ETH_SPI_CS0_GPIO,
    ETH_SPI_INT0_GPIO,
    ETH_SPI_PHY_RST0_GPIO,
    EXT_SPI_CS1_GPIO,
};

const size_t num_spi_lines = sizeof(spi_lines) / sizeof(spi_lines[0]);

// I2C lines sleep
const gpio_num_t i2c_lines[] = {
    I2C_SCL,
    I2C_SDA,
};

const size_t num_i2c_lines = sizeof(i2c_lines) / sizeof(i2c_lines[0]);

// EXT ADC lines 0 and 1
const gpio_num_t ext_dio_adc[] = {
    EXT_DIO_ADC_GPIO0,
    EXT_DIO_ADC_GPIO1,
};

const size_t num_ext_dio_adc = sizeof(ext_dio_adc) / sizeof(ext_dio_adc[0]);

// INPUT GPIOS
const gpio_num_t gpios_input[] = {
    SETTING_BTN9,
};
const size_t num_gpios_input = sizeof(gpios_input) / sizeof(gpios_input[0]);

void disable_holds(void)
{
    ESP_LOGI(TAG, "Disabling holds");
    dis_hold_gpio(gpios_output, num_gpios_output);
}

void reset_gpios_dis_hold(void)
{
    ESP_LOGI(TAG, "Disabling holds");
    reset_gpios(gpios_to_reset, num_gpios_to_reset);
}

void set_wake_configs(void)
{
    // Configure dynamic frequency scaling:
    // maximum and minimum frequencies are set in sdkconfig,
    // automatic light sleep is enabled if tickless idle support is enabled.
   esp_pm_config_t pm_config = {
        .max_freq_mhz = 160,
        .min_freq_mhz = 80,
        .light_sleep_enable = false,
    };
    ESP_LOGW(TAG, "Starting WAKE esp_pm_configure!");
	ESP_ERROR_CHECK_WITHOUT_ABORT(esp_pm_configure(&pm_config));
	ESP_LOGW(TAG, "Finished WAKE esp_pm_configure!");

    ESP_LOGW(TAG, "Setting WAKE configs");
    esp_wifi_set_ps(DEFAULT_PS_MODE);
    //led_initialize();
    //led_off();
    //ldo_off();
    //eth_pwr_off();

    // call the function to set GPIOs to output with internal pullup/down disabled
    // set_and_hold_gpio_outputs_low(gpios_output, num_gpios_output);
    // set_and_hold_gpio_outputs_low(spi_lines, num_spi_lines);
    // set_and_hold_gpio_outputs_low(i2c_lines, num_i2c_lines);
    // set_and_hold_gpio_outputs_low(ext_dio_adc, num_ext_dio_adc);
    // set_gpios_to_input_pulldown(spi_lines, num_spi_lines);
    // set_gpios_to_input_pulldown(i2c_lines, num_i2c_lines);
}

void set_sleep_configs(void)
{
    
    led_off();
    ldo_off();
    eth_pwr_off();

    // Configure dynamic frequency scaling:
    // maximum and minimum frequencies are set in sdkconfig,
    // automatic light sleep is enabled if tickless idle support is enabled.
    //esp_pm_config_t pm_config = {
    //     .max_freq_mhz = 160,
    //     .min_freq_mhz = 80,
    //     //.light_sleep_enable = true,
    // };
    // ESP_LOGW(TAG, "Starting LIGHTSLEEP esp_pm_configure!");
	// ESP_ERROR_CHECK_WITHOUT_ABORT(esp_pm_configure(&pm_config));
	// ESP_LOGW(TAG, "Finished LIGHTSLEEP esp_pm_configure!");


    ESP_LOGW(TAG, "Setting sleep configs");
    esp_wifi_set_ps(DEFAULT_PS_MODE);

    // call the function to set GPIOs to output with internal pullup/down disabled
    set_and_hold_gpio_outputs_low(gpios_output, num_gpios_output);
    // set_and_hold_gpio_outputs_low(spi_lines, num_spi_lines);
    set_and_hold_gpio_outputs_low(i2c_lines, num_i2c_lines);
    set_and_hold_gpio_outputs_low(ext_dio_adc, num_ext_dio_adc);
    // set_gpios_to_input_pulldown(spi_lines, num_spi_lines);
    set_gpios_to_input_pulldown(i2c_lines, num_i2c_lines);
}
