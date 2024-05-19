#include "app_wifi.h"
#include "sensors.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "driver/temperature_sensor.h"
#include "esp_log.h"
#include "inttypes.h"
#include "buzzer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "soc/soc_caps.h"
#include "device_config.h"
#include "driver/gpio.h"
#include "device_config.h"
#include "ldo_control.h"
#include "led.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define INT_STRING_SIZE 10 // Adjust the size as per your requirement

static const char *TAG = "SENSORS";

temperature_sensor_handle_t temp_sensor_handle = NULL;

static adc_cali_handle_t adc_cali_handle = NULL;
static adc_oneshot_unit_handle_t adc_handle = NULL;

void temperature_sensor_init()
{
    esp_err_t ret;

    // temperature_sensor_config_t temp_sensor_config = {
    //     .range_min = -10,
    //     .range_max = 80,
    // };

    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);

    ret = temperature_sensor_install(&temp_sensor_config, &temp_sensor_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to install temperature sensor %d", ret);
        // Decide how to handle the error, e.g., continue, break, return
    }
    else
    {
        ESP_LOGI(TAG, "Temperature sensor initialized & confiured");
    }
}

void temperature_sensor_en(bool enable)
{
    esp_err_t ret;
    if (enable)
    {
        ESP_LOGI(TAG, "Enabling temperature sensor");
        ret = temperature_sensor_enable(temp_sensor_handle);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to enable temperature sensor %d", ret);
            // Decide how to handle the error, e.g., continue, break, return
        }
        else
        {
            ESP_LOGI(TAG, "Temperature sensor enabled");
        }
    }
    else
    {
        ESP_LOGI(TAG, "Disable temperature sensor");
        ret = temperature_sensor_disable(temp_sensor_handle);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "failed to disable temperature sensor %d", ret);
            // Decide how to handle the error, e.g., continue, break, return
        }
        else
        {
            ESP_LOGI(TAG, "Temperature sensor disabled");
        }
    }
}

void batt_sensor_en(bool enable)
{
    // Initialization code remains the same
    // gpio_pad_select_gpio(LDO_CONTROL_GPIO);
    gpio_set_direction(BATT_SENSE_EN_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(BATT_SENSE_EN_GPIO, enable ? 1 : 0);
}

static char *int_to_string(int value)
{
    static char str[INT_STRING_SIZE];
    snprintf(str, INT_STRING_SIZE, "%d", value);
    return str;
}

char *get_board_temp()
{
    temperature_sensor_en(true);
    char *out_temp = "nan";
    float tsens_out;

    vTaskDelay(pdMS_TO_TICKS(1)); // Delay to fix the temperature sensor reading issue

    if (temperature_sensor_get_celsius(temp_sensor_handle, &tsens_out) != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading temperature data");
        return out_temp;
    }
    // ESP_LOGI(TAG, "Temperature %.02f ℃", tsens_out);
    int temperature = (int)tsens_out; // Convert float to int
    ESP_LOGI(TAG, "Temperature out celsius %d°C", temperature);
    out_temp = int_to_string(temperature);
    temperature_sensor_en(false);
    return out_temp;
}

void adc_en(bool enable)
{
    esp_err_t ret;

    if (enable)
    {
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT_1,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

        adc_oneshot_chan_cfg_t channel_config = {
            .bitwidth = BAT_ADC_WIDTH,
            .atten = BAT_ADC_ATTEN,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, BATT_SENSE_ADC, &channel_config));

        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .atten = BAT_ADC_ATTEN,
            .bitwidth = BAT_ADC_WIDTH,
        };
        ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle));
    }
    else
    {
        // Deinitialize ADC on disabling
        if (adc_handle)
        {
            ret = adc_oneshot_del_unit(adc_handle);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "failed to disable ADC %d", ret);
                // Decide how to handle the error, e.g., continue, break, return
            }
            else
            {
                // ESP_LOGI(TAG, "ADC disabled");
                adc_handle = NULL;
            }
        }
    }
}

void get_power_volts(float *scaled_voltage, int *adc_reading)
{
    *adc_reading = 0;
    for (int i = 0; i < BAT_SAMPLES; i++)
    {
        int raw;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, BATT_SENSE_ADC, &raw));
        *adc_reading += raw;
    }
    *adc_reading /= BAT_SAMPLES;

    /*
        //float voltage = (adc_reading * 1.5) / (1 << ADC_WIDTH);

        // Calibrate based on experiments:
        //voltage -= 0.07;
    */

    int voltage;
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, *adc_reading, &voltage));
    *scaled_voltage = (voltage * 6.0) / 0.5;

    // Calibrate based on experiments. This probably changes based on
    // on how the components in the voltage divider actually perform
    // and loss due to breadboard.
    *scaled_voltage /= 1.08;

    printf("Raw ADC value: %ls\tVoltage: %d mV\tScaled Voltage: %f mV\n", adc_reading, voltage, *scaled_voltage);
}

// void get_power_volts(float *scaled_voltage, int *raw_value)
// {
//     uint32_t adc_reading = 0;
//     *raw_value = 0;

//     gpio_hold_dis(LDO_CONTROL_GPIO);
//     ldo_on();

//     // Enable ADC
//     // adc_en(true);
//     vTaskDelay(pdMS_TO_TICKS(100));

//     // Multisampling
//     for (int i = 0; i < NO_OF_SAMPLES; i++)
//     {
//         ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, BATT_SENSE_ADC, raw_value));
//         adc_reading += *raw_value;
//     }
//     adc_reading /= NO_OF_SAMPLES;

//     // Calculate voltage
//     *voltage = (adc_reading * DEFAULT_VREF) / 4096;

//     // Optional: Log the result
//     ESP_LOGW("ADC", "Raw: %d\tVoltage: %lu mV", *raw_value, *voltage);

//     // adc_en(false);
// }