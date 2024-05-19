#include "uart_handler.h"
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

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


static const char *TAG = "RS485_Handler";
static char latest_valid_data[BUF_SIZE];
static time_t last_valid_data_time = 0;
TaskHandle_t uart_data_task_handle = NULL;
static volatile bool isUartReading = false;

#define DATA_VALIDITY_PERIOD 60 // Data is considered valid for 60 seconds

static SemaphoreHandle_t mySemaphore = NULL;


// // Configuration parameters (ensure they are defined correctly)
// #define RS485_TXD   (5)
// #define RS485_RXD   (4)
// #define RS485_RTS   (7)
// #define RS485_CTS   (UART_PIN_NO_CHANGE)
// #define BUF_SIZE    (127)
// #define BAUD_RATE   (38400)
// #define PACKET_READ_TICS (100 / portTICK_PERIOD_MS)
// #define RS485_TASK_STACK_SIZE (2048)
// #define RS485_TASK_PRIO (10)
// #define RS485_UART_PORT (1)
// #define RS485_READ_TOUT (3)


// Helper function to initialize the semaphore
static void initializeSemaphoreIfNeeded() {
    if (mySemaphore == NULL) {
        SemaphoreHandle_t tempHandle = xSemaphoreCreateMutex();
        if (tempHandle != NULL) {
            // Attempt to set the semaphore handle if it's still NULL
            if (__sync_bool_compare_and_swap(&mySemaphore, NULL, tempHandle)) {
                // Success: The semaphore handle is now initialized
            } else {
                // Another task initialized it first, delete the redundant semaphore
                vSemaphoreDelete(tempHandle);
            }
        }
    }
}


bool uart_initialize() {
    uint32_t uart_read_baudrate = BAUD_RATE; // Example fixed baud rate, adjust as necessary

    const int uart_num = RS485_UART_PORT;
    uart_config_t uart_config = {
        .baud_rate = uart_read_baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_LOGI(TAG, "*Initializing RS485 with Baudrate %lu", uart_read_baudrate);

    ESP_ERROR_CHECK(uart_driver_install(uart_num, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(uart_num, RS485_TXD, RS485_RXD, RS485_RTS, RS485_CTS));
    ESP_ERROR_CHECK(uart_set_mode(uart_num, UART_MODE_RS485_HALF_DUPLEX));
    ESP_ERROR_CHECK(uart_set_rx_timeout(uart_num, RS485_READ_TOUT));

    return true;
}

bool uart_remove_driver() {
    const int uart_num = RS485_UART_PORT;
    if (uart_driver_delete(uart_num) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to remove UART driver");
        return false;
    }
    ESP_LOGI(TAG, "Removed UART driver");
    return true;
}

bool get_uart_data(char *buffer, size_t buffer_size) {
    isUartReading = true;
    uint8_t *data = (uint8_t *)malloc(BUF_SIZE);
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate memory for data buffer");
        return false;
    }

    int len = 0;

    const char *test_str = "3101valuer\n\r";
    uart_flush(RS485_UART_PORT);
    uart_write_bytes(RS485_UART_PORT, test_str, strlen(test_str));
    ESP_ERROR_CHECK(uart_wait_tx_done(RS485_UART_PORT, PACKET_READ_TICS));

    len = uart_read_bytes(RS485_UART_PORT, data, BUF_SIZE, PACKET_READ_TICS);

    if (len > 0) { // Assuming validation is based on receiving any data
        ESP_LOGI(TAG, "Received valid data: %.*s", len, (char *)data);
        snprintf(buffer, buffer_size, "%.*s", len, (char *)data);
        led_green_on(); // Indicate valid data received
        free(data);
        isUartReading = false;
        return true;
    } else {
        led_red_on(); // Indicate no valid data received
        ESP_LOGI(TAG, "No valid data received");
    }

    free(data);
    isUartReading = false;
    return false;
}

static void uart_data_task(void *pvParameters) {
    char data_buffer[BUF_SIZE];
    if (get_uart_data(data_buffer, BUF_SIZE)) {
        strncpy(latest_valid_data, data_buffer, BUF_SIZE);
        last_valid_data_time = time(NULL); // Update timestamp
        uart_flush(RS485_UART_PORT);
        ESP_LOGI(TAG, "Valid data received, exiting task.");
        vTaskDelete(NULL); // Exit task after receiving valid data
    }
}

const char *get_valid_uart_data() {
    // Ensure the semaphore is initialized before use
    initializeSemaphoreIfNeeded();

    if (xSemaphoreTake(mySemaphore, portMAX_DELAY) == pdTRUE) {
        // Protected section
        if ((time(NULL) - last_valid_data_time) > DATA_VALIDITY_PERIOD) {
            xSemaphoreGive(mySemaphore); // Always release the semaphore
            return "nan";
        }

        ESP_LOGI(TAG, "Latest valid UART data: %s", latest_valid_data);
        xSemaphoreGive(mySemaphore); // Always release the semaphore
        return latest_valid_data;
    }

    // Semaphore taking failed, which should not happen with portMAX_DELAY
    return "nan";
}

void start_uart_data_task() {
    stop_uart_data_task(); // Ensure any existing task is stopped
    disable_holds();
    ldo_on();
    ESP_LOGW(TAG, "Starting RS485 task...");
    uart_initialize();
    xTaskCreate(uart_data_task, "uart_data_task", RS485_TASK_STACK_SIZE, NULL, RS485_TASK_PRIO, &uart_data_task_handle);
}

void stop_uart_data_task() {
    if (uart_data_task_handle != NULL) {
        ESP_LOGW(TAG, "Stopping RS485 task...");
        vTaskDelete(uart_data_task_handle);
        uart_data_task_handle = NULL;
        uart_remove_driver();
        ldo_off();
    }
}
