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
static adc_oneshot_unit_handle_t adc1_handle = NULL;

static int adc_raw[2][10];
static int voltage[2][10];
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void example_adc_calibration_deinit(adc_cali_handle_t handle);

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
        //-------------ADC1 Init---------------//
        adc_oneshot_unit_init_cfg_t init_config1 = {
            .unit_id = ADC_UNIT_1,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

        //-------------ADC1 Config---------------//
        adc_oneshot_chan_cfg_t config = {
            .bitwidth = BAT_ADC_WIDTH,
            .atten = BAT_ADC_ATTEN,
        };
        // ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN0, &config));
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, BAT_ADC_CHANNEL, &config));

        //-------------ADC1 Calibration Init---------------//
        // bool do_calibration1_chan0 = example_adc_calibration_init(ADC_UNIT_1, EXAMPLE_ADC1_CHAN0, EXAMPLE_ADC_ATTEN, &adc1_cali_chan0_handle);
    }
    else
    {
        // Deinitialize ADC on disabling
        if (adc1_handle)
        {
            ret = adc_oneshot_del_unit(adc1_handle);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "failed to disable ADC %d", ret);
                // Decide how to handle the error, e.g., continue, break, return
            }
            else
            {
                // ESP_LOGI(TAG, "ADC disabled");
                adc1_handle = NULL;
            }
        }
    }
}

void get_power_volts(float *scaled_voltage, int *adc_reading)
{
    // ESP_LOGI(TAG, "Wake up from timer. Disabling GPIO hold.");
    // gpio_hold_dis(GPIO_NUM_16);
    // gpio_hold_dis(GPIO_NUM_14);

    // // Configure GPIO 16 as output and set it to high
    // gpio_reset_pin(GPIO_NUM_16); // Reset the pin
    // gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT);
    // gpio_set_level(GPIO_NUM_16, 1); // Set GPIO 16 high

    // // Configure GPIO 14 as output and set it to high
    // gpio_reset_pin(GPIO_NUM_14); // Reset the pin
    // gpio_set_direction(GPIO_NUM_14, GPIO_MODE_OUTPUT);
    // gpio_set_level(GPIO_NUM_14, 1); // Set GPIO 14 high

    bool do_calibration1_chan1 = example_adc_calibration_init(ADC_UNIT_1, BAT_ADC_CHANNEL, BAT_ADC_ATTEN, &adc_cali_handle);

    for (int i = 0; i < BAT_SAMPLES; i++)
    {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, BATT_SENSE_ADC, &adc_raw[0][1]));
        if (do_calibration1_chan1)
        {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, adc_raw[0][1], &voltage[0][1]));
            ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, BATT_SENSE_ADC, voltage[0][1]);
        }

        *adc_reading += adc_raw[0][1];
        *scaled_voltage += voltage[0][1];
    }
    *adc_reading /= BAT_SAMPLES;
    *scaled_voltage /= BAT_SAMPLES;

    *scaled_voltage = *scaled_voltage + 20; // Convert mV to V
    *adc_reading = *scaled_voltage + 2020;  // Convert mV to V  
    /*
        //float voltage = (adc_reading * 1.5) / (1 << ADC_WIDTH);

        // Calibrate based on experiments:
        //voltage -= 0.07;
    */
    ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, BATT_SENSE_ADC, voltage[0][1]);
    ESP_LOGI(TAG, "ADC%d Channel[%d] Raw ADC value: %d\tVoltage: %d mV\tScaled Voltage: %f mV", ADC_UNIT_1 + 1, BATT_SENSE_ADC, adc_raw[0][1], voltage[0][1], *scaled_voltage);
    
}

/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Calibration Success");
    }
    else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated)
    {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    }
    else
    {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

static void example_adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
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