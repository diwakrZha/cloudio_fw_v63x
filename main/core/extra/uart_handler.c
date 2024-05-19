#include "uart_handler.h"
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "string.h"
#include "driver/gpio.h"
#include "led.h"
#include "aws_config_handler.h"
#include "vinit.h"
#include "inttypes.h"
#include "device_config.h"
#include "ldo_control.h"
#include "psm_routine.h"

/**
 * This is a example which echos any data it receives on UART back to the sender using RS485 interface in half duplex mode.
 */
static const char *TAG = "RS485_Handler";

static char latest_valid_data[BUF_SIZE];
static time_t last_valid_data_time = 0;
TaskHandle_t uart_data_task_handle = NULL;
static volatile bool isUartReading = false;

// // Note: Some pins on target chip cannot be assigned for UART communication.
// // Please refer to documentation for selected board and target to configure pins using Kconfig.
// #define RS485_TXD   (5) //board has weirdness about this, strangely, this is reversed.
// #define RS485_RXD   (4)

// // RTS for RS485 Half-Duplex Mode manages DE/~RE
// #define RS485_RTS   (7)

// // CTS is not used in RS485 Half-Duplex Mode
// #define RS485_CTS   (UART_PIN_NO_CHANGE)

// #define BUF_SIZE        (127)
// #define BAUD_RATE       (38400)

// // Read packet timeout
// #define PACKET_READ_TICS        (100 / portTICK_PERIOD_MS)
// #define RS485_TASK_STACK_SIZE    (2048)
// #define RS485_TASK_PRIO          (10)
// #define RS485_UART_PORT          (1)

// // Timeout threshold for UART = number of symbols (~10 tics) with unchanged state on receive pin
// #define RS485_READ_TOUT          (3) // 3.5T * 8 = 28 ticks, TOUT=3 -> ~24..33 ticks

bool uart_initialize()
{
    // uint32_t uart_read_baudrate = (conf.bdrt == 0) ? DEFAULT_BAUD_RATE : conf.bdrt;
    uint32_t uart_read_baudrate = conf.bdrt;

    const int uart_num = RS485_UART_PORT;
    uart_config_t uart_config = {
        .baud_rate = BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "*Initializing RS485 with Baudrate %lu \n", uart_read_baudrate);
    ESP_LOGI(TAG, "*Query String %s \n", conf.qry_str);

    // Install UART driver (we don't need an event queue here)
    // In this example we don't even use a buffer for sending data.
    ESP_ERROR_CHECK(uart_driver_install(uart_num, BUF_SIZE * 2, 0, 0, NULL, 0));

    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));

    ESP_LOGI(TAG, "UART set pins, mode and install driver.");

    // Set UART pins as per KConfig settings
    ESP_ERROR_CHECK(uart_set_pin(uart_num, RS485_TXD, RS485_RXD, RS485_RTS, RS485_CTS));

    // Set RS485 half duplex mode
    ESP_ERROR_CHECK(uart_set_mode(uart_num, UART_MODE_RS485_HALF_DUPLEX));

    // Set read timeout of UART TOUT feature
    ESP_ERROR_CHECK(uart_set_rx_timeout(uart_num, RS485_READ_TOUT));

    return true;
}

bool uart_remove_driver()
{
    const int uart_num = RS485_UART_PORT;
    if (uart_driver_delete(uart_num) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to remove UART driver");
        return false;
    }
    ESP_LOGI(TAG, "Removed UART driver");
    return true;
}

static bool get_uart_data(char *buffer, size_t buffer_size)
{
    isUartReading = true;
    uint8_t *data = (uint8_t *)malloc(BUF_SIZE);
    if (data == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for data buffer");
        return false;
    }

    int len = 0;

    char *querry_str = conf.qry_str;
    //char *querry_str = "3101valuer\n\r";
    //char *querry_str = "000";
    //uint32_t uart_read_baudrate = conf.bdrt;
    ESP_LOGW(TAG, "*Query String active %s \n", querry_str);

    for (int ctr = 0; ctr < 10; ctr++)
    {
        uart_flush(RS485_UART_PORT);
        vTaskDelay(pdMS_TO_TICKS(50));

        uart_write_bytes(RS485_UART_PORT, querry_str, strlen(querry_str));
        ESP_ERROR_CHECK(uart_wait_tx_done(RS485_UART_PORT, 10));
        vTaskDelay(pdMS_TO_TICKS(100));

        ESP_ERROR_CHECK(uart_get_buffered_data_len(RS485_UART_PORT, (size_t *)&len));

        len = uart_read_bytes(RS485_UART_PORT, data, BUF_SIZE, PACKET_READ_TICS);
        ESP_LOGW(TAG, "*Query String active %s \n", querry_str);
        ESP_LOGI(TAG, "data length: %d", len);
        ESP_LOGI(TAG, "Received data_0: %s \n", (char *)data);


        if (len >= 20)
        {
            ESP_LOGI(TAG, "Received data: %s \nData looks valid", (char *)data);
            snprintf(buffer, buffer_size, "%.*s", len, (char *)data);
            free(data);
            uart_flush(RS485_UART_PORT);
            led_green_on();
            vTaskDelay(pdMS_TO_TICKS(10));
            isUartReading = false;
            return true;
        }
        else
        {
            led_red_on();
            ESP_LOGI(TAG, "Invalid data: %s \nAttempt: %d...", (char *)data, ctr);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
        led_off();
    }
    free(data);
    uart_flush(RS485_UART_PORT);
    isUartReading = false;
    vTaskDelay(pdMS_TO_TICKS(10));
    return false;
}

static void uart_data_task(void *pvParameters)
{
    char data_buffer[BUF_SIZE];
    while (1)
    {
        if (get_uart_data(data_buffer, BUF_SIZE))
        {
            strncpy(latest_valid_data, data_buffer, BUF_SIZE);
            last_valid_data_time = time(NULL); // Update timestamp
            uart_flush(RS485_UART_PORT);
            ESP_LOGI(TAG, "Stopping RS485 task...");
            
            //vTaskDelete(uart_data_task_handle);
            //stop_uart_data_task();

            //break;
        }
        vTaskDelay(pdMS_TO_TICKS(GET_UART_RETRY_INTERVAL)); // 20s delay
    }
}

const char *get_valid_uart_data()
{
    if ((time(NULL) - last_valid_data_time) > 120)
    { // 2 minutes
        return "nan";
    }
    ESP_LOGI(TAG, "Latest valid UART data: %s \n", latest_valid_data);
    return latest_valid_data;
}

void start_uart_data_task()
{
    stop_uart_data_task();
    //reset_gpios_dis_hold();
    ldo_on();
    ESP_LOGW(TAG, "Starting RS485 task...");
    uart_initialize();
    if (uart_data_task_handle == NULL)
    {
        xTaskCreate(uart_data_task, "uart_data_task", 2048, NULL, 1, &uart_data_task_handle);
    }
}

void stop_uart_data_task()
{
    while (isUartReading)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (uart_data_task_handle != NULL)
    {
        ESP_LOGW(TAG, "Starting RS485 task...");
        vTaskDelete(uart_data_task_handle);
        uart_data_task_handle = NULL;
        uart_remove_driver();
        ldo_off();
    }
}
