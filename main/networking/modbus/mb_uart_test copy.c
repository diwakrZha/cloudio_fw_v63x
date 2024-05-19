/*
 * SPDX-FileCopyrightText: 2016-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "string.h"
#include "esp_log.h"
#include "modbus_params.h" // for modbus parameters structures
#include "mbcontroller.h"
#include "sdkconfig.h"
#include "packed_data_handler.h"

#define MB_PORT_NUM (CONFIG_MB_UART_PORT_NUM)   // Number of UART port used for Modbus connection
#define MB_DEV_SPEED (CONFIG_MB_UART_BAUD_RATE) // The communication speed of the UART

// Note: Some pins on target chip cannot be assigned for UART communication.
// See UART documentation for selected board and target to configure pins using Kconfig.

// The number of parameters that intended to be used in the particular control process
#define MASTER_MAX_CIDS num_device_parameters

// Number of reading of parameters from slave
#define MASTER_MAX_RETRY 30

// Timeout to update cid over Modbus
#define UPDATE_CIDS_TIMEOUT_MS (500)
#define UPDATE_CIDS_TIMEOUT_TICS (UPDATE_CIDS_TIMEOUT_MS / portTICK_PERIOD_MS)

// Timeout between polls
#define POLL_TIMEOUT_MS (1)
#define POLL_TIMEOUT_TICS (POLL_TIMEOUT_MS / portTICK_PERIOD_MS)

// The macro to get offset for parameter in the appropriate structure
#define HOLD_OFFSET(field) ((uint16_t)(offsetof(holding_reg_params_t, field) + 1))
#define INPUT_OFFSET(field) ((uint16_t)(offsetof(input_reg_params_t, field) + 1))
#define COIL_OFFSET(field) ((uint16_t)(offsetof(coil_reg_params_t, field) + 1))
// Discrete offset macro
#define DISCR_OFFSET(field) ((uint16_t)(offsetof(discrete_reg_params_t, field) + 1))

#define STR(fieldname) ((const char *)(fieldname))
// Options can be used as bit masks or parameter limits
#define OPTS(min_val, max_val, step_val)                   \
    {                                                      \
        .opt1 = min_val, .opt2 = max_val, .opt3 = step_val \
    }

static const char *TAG = "MASTER_TEST";

// Enumeration of modbus device addresses accessed by master device
enum
{
    MB_DEVICE_ADDR1 = 001 // Only one slave device used for the test (add other slave addresses here)
};

// Enumeration of all supported CIDs for device (used in parameter definition table)
enum
{
    CID_INP_DATA_0 = 0,
    CID_HOLD_DATA_0,
    CID_INP_DATA_1,
    CID_HOLD_DATA_1,
    CID_INP_DATA_2,
    CID_HOLD_DATA_2,
    CID_HOLD_TEST_REG,
    CID_RELAY_P1,
    CID_RELAY_P2,
    CID_COUNT
};

#define UART_LOG_TAG "UART_LOG"
#define RX_BUF_SIZE 512

#define MB_READ_REGISTERS_MAX 10 // Max number of registers we attempt to read at once

#define MB_PAR_INFO_GET_TOUT (10) // Timeout for parameter get information

// Utility function to convert error code to string; implement as needed
const char *esp_err_to_name(esp_err_t code);


static const char* MD_TAG = "MODBUS_TASK_2";


#define MB_MASTER_TIMEOUT_MS        (100)
#define MB_READ_REGISTERS_ADDR      (000) // Starting address of registers
#define MB_READ_REGISTERS_COUNT     (100)   // Number of registers to read

void read_holding_registers_from_slave() {
    uint8_t slave_address = 1; // Example slave address
    uint16_t start_register = MB_READ_REGISTERS_ADDR;
    uint16_t register_count = MB_READ_REGISTERS_COUNT;
    uint16_t data[register_count]; // Allocate memory for register values

    for (int i = 0; i < register_count; ++i) {
        uint16_t current_register = start_register + i;
        esp_err_t err = mbc_master_get_parameter(current_register, NULL, (uint8_t*)&data[i], NULL);
        if (err != ESP_OK) {
            ESP_LOGE("MODBUS", "mbc_master_get_parameter failed for register %d: (%s).", current_register, esp_err_to_name(err));
            return;
        }
        if (data[i] == 0xFFFF) {
            ESP_LOGW("MODBUS", "Register [%d] value is invalid.", current_register);
        }
    }

    // Successfully read the registers
    for (int i = 0; i < register_count; ++i) {
        ESP_LOGI("MODBUS", "Register [%d] = %u", (start_register + i), data[i]);
    }
}


void modbus_task(void *pvParameters) {
    // Delay to allow initialization of modbus master
    vTaskDelay(pdMS_TO_TICKS(1000));

    while (1) {
        read_holding_registers_from_slave();
        vTaskDelay(pdMS_TO_TICKS(100)); // Read every 5 seconds for example
    }
}


// Simplified function to discover and log Modbus data
void discover_modbus_parameters(void)
{
    esp_err_t err;
    uint16_t start_cid = 0; // Starting CID
    uint16_t end_cid = 50;  // End CID, adjust based on your needs
    char temp_key[16];      // Temporary key for logging, adjust as needed
    uint8_t value[4];       // Assuming a maximum of 4 bytes per parameter
    uint8_t type;

    for (uint16_t cid = start_cid; cid <= end_cid; cid++)
    {
        sprintf(temp_key, "CID_%u", cid); // Create a temporary key for each CID
        err = mbc_master_get_parameter(cid, temp_key, value, &type);

        if (err == ESP_OK)
        {
            // Log successful read
            ESP_LOGI("DISCOVER parameters: ", "Successfully read CID %u: %s", cid, temp_key);
        }
        else
        {
            // Log failure but continue to the next CID
            ESP_LOGE("DISCOVER parameters:", "Failed to read CID %u: %s", cid, temp_key);
        }
    }
}

// Call this function where appropriate in your main or initialization code
// discover_modbus_parameters();

void log_decoded_uart_data(uint8_t *rx_buffer, int length) {
    // Check if the length is even (assuming 16-bit integers)
    if (length % 2 != 0) {
        ESP_LOGE(TAG, "Invalid data length for 16-bit integers.");
        return;
    }

    // Decode and log the 16-bit integers
    ESP_LOGI(TAG, "Decoded data from Modbus registers:");
    for (int i = 0; i < length; i += 2) {
        uint16_t value;
        memcpy(&value, &rx_buffer[i], sizeof(uint16_t));
        ESP_LOGI(TAG, "Register %d: %u", i / 2, value);
    }
}



void uart_log_task(void *params) {
    uint8_t *data = (uint8_t *)malloc(RX_BUF_SIZE);
    if (!data) {
        ESP_LOGE(UART_LOG_TAG, "Failed to allocate memory for RX buffer");
        vTaskDelete(NULL);
        return;
    }

    char *packed_data = NULL; // Initialize packed_data pointer

    while (1) {
        const int len = uart_read_bytes(MB_PORT_NUM, data, RX_BUF_SIZE, 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            // Pack data delimited by hashes
            char packed_data_buffer[3 * len]; // Assuming each byte will take up to 3 characters
            int packed_len = 0;
            for (int i = 0; i < len; i++) {
                if (i > 0) {
                    packed_data_buffer[packed_len++] = '#'; // Hash delimiter
                }
                packed_len += sprintf(&packed_data_buffer[packed_len], "%d", data[i]); // Pack byte as string
            }
            packed_data_buffer[packed_len] = '\0'; // Null-terminate the string

            // Assign packed_data_buffer to packed_data pointer
            packed_data = strdup(packed_data_buffer);

            // Process the packed data
            handle_packed_data(data, len);

            // Stop the task if the byte size exceeds 50 bytes
            if (packed_len > 50) {
                ESP_LOGI(UART_LOG_TAG, "Data size exceeds 50 bytes. Stopping task.");
                break;
            }
        }

        // Minimized delay to ensure responsiveness
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    // Return the packed data to main.c
    process_packed_data(packed_data);

    free(data);
    free(packed_data); // Free the dynamically allocated memory
    vTaskDelete(NULL);
}



void log_raw_uart_data()
{
    uint8_t rx_buffer[2048]; // Adjust size as necessary
    int length = 0;

    // Assuming you have a function to read from the UART buffer, like uart_read_bytes for ESP-IDF
    // This is a hypothetical function call; actual implementation may vary.
    length = uart_read_bytes(MB_PORT_NUM, rx_buffer, sizeof(rx_buffer), 20 / portTICK_PERIOD_MS);
    if (length > 0)
    {
        ESP_LOGI(TAG, "Raw response data (%d bytes):", length);
        // Log data in decimal format
        for (int i = 0; i < length; i++)
        {
            ESP_LOGI(TAG, "%d", rx_buffer[i]);
        }

        // Decode and log the Modbus register data
        log_decoded_uart_data(rx_buffer, length);
    }
    else
    {
        ESP_LOGE(TAG, "No data available or read error.");
    }
}


#define RX_BUF_SIZE 512
#define MB_PORT_NUM 1 // Example UART port number
#define UART_LOG_TAG "UART_LOG"

// Simulating UART read function for demonstration
int uart_read_bytes(int port, uint8_t *data, int size, int timeout) {
    // Simulate UART read here
    return 10; // Simulated data length
}

// Function to pack data and return as a string
char* get_packed_data() {
    uint8_t *data = (uint8_t *)malloc(RX_BUF_SIZE);
    if (!data) {
        printf("Failed to allocate memory for RX buffer\n");
        return NULL;
    }

    char *packed_data = NULL;
    int total_bytes = 0;

    // Run the loop until data exceeds 50 bytes
    while (total_bytes <= 50) {
        const int len = uart_read_bytes(MB_PORT_NUM, data, RX_BUF_SIZE, 20);
        if (len > 0) {
            // Pack data delimited by hashes
            char packed_data_buffer[3 * len]; // Assuming each byte will take up to 3 characters
            int packed_len = 0;
            for (int i = 0; i < len; i++) {
                if (i > 0) {
                    packed_data_buffer[packed_len++] = '#'; // Hash delimiter
                }
                packed_len += sprintf(&packed_data_buffer[packed_len], "%d", data[i]); // Pack byte as string
            }
            packed_data_buffer[packed_len] = '\0'; // Null-terminate the string

            // Concatenate packed data
            if (packed_data == NULL) {
                packed_data = strdup(packed_data_buffer);
            } else {
                packed_data = realloc(packed_data, strlen(packed_data) + strlen(packed_data_buffer) + 1);
                strcat(packed_data, packed_data_buffer);
            }

            total_bytes += len;
        } else {
            break; // No more data available
        }
    }

    free(data);

    return packed_data;
}



// Example Data (Object) Dictionary for Modbus parameters:
// The CID field in the table must be unique.
// Modbus Slave Addr field defines slave address of the device with correspond parameter.
// Modbus Reg Type - Type of Modbus register area (Holding register, Input Register and such).
// Reg Start field defines the start Modbus register number and Reg Size defines the number of registers for the characteristic accordingly.
// The Instance Offset defines offset in the appropriate parameter structure that will be used as instance to save parameter value.
// Data Type, Data Size specify type of the characteristic and its data size.
// Parameter Options field specifies the options that can be used to process parameter value (limits or masks).
// Access Mode - can be used to implement custom options for processing of characteristic (Read/Write restrictions, factory mode values and etc).
const mb_parameter_descriptor_t device_parameters[] = {
    // { CID, Param Name, Units, Modbus Slave Addr, Modbus Reg Type, Reg Start, Reg Size, Instance Offset, Data Type, Data Size, Parameter Options, Access Mode}
    {CID_INP_DATA_0, STR("Data_channel_0"), STR("Volts"), MB_DEVICE_ADDR1, MB_PARAM_INPUT, 0, 2,
     INPUT_OFFSET(input_data0), PARAM_TYPE_FLOAT, 4, OPTS(-10, 10, 1), PAR_PERMS_READ_WRITE_TRIGGER},
    {CID_HOLD_DATA_0, STR("Humidity_1"), STR("%rH"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 0, 2,
     HOLD_OFFSET(holding_data0), PARAM_TYPE_FLOAT, 4, OPTS(0, 100, 1), PAR_PERMS_READ_WRITE_TRIGGER},
    {CID_INP_DATA_1, STR("Temperature_1"), STR("C"), MB_DEVICE_ADDR1, MB_PARAM_INPUT, 2, 2,
     INPUT_OFFSET(input_data1), PARAM_TYPE_FLOAT, 4, OPTS(-40, 100, 1), PAR_PERMS_READ_WRITE_TRIGGER},
    {CID_HOLD_DATA_1, STR("Humidity_2"), STR("%rH"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 2, 2,
     HOLD_OFFSET(holding_data1), PARAM_TYPE_FLOAT, 4, OPTS(0, 100, 1), PAR_PERMS_READ_WRITE_TRIGGER},
    {CID_INP_DATA_2, STR("Temperature_2"), STR("C"), MB_DEVICE_ADDR1, MB_PARAM_INPUT, 4, 2,
     INPUT_OFFSET(input_data2), PARAM_TYPE_FLOAT, 4, OPTS(-40, 100, 1), PAR_PERMS_READ_WRITE_TRIGGER},
    {CID_HOLD_DATA_2, STR("Humidity_3"), STR("%rH"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 4, 2,
     HOLD_OFFSET(holding_data2), PARAM_TYPE_FLOAT, 4, OPTS(0, 100, 1), PAR_PERMS_READ_WRITE_TRIGGER},
    {CID_HOLD_TEST_REG, STR("Test_regs"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 10, 58,
     HOLD_OFFSET(test_regs), PARAM_TYPE_ASCII, 116, OPTS(0, 100, 1), PAR_PERMS_READ_WRITE_TRIGGER},
    {CID_RELAY_P1, STR("RelayP1"), STR("on/off"), MB_DEVICE_ADDR1, MB_PARAM_COIL, 0, 8,
     COIL_OFFSET(coils_port0), PARAM_TYPE_U16, 2, OPTS(BIT1, 0, 0), PAR_PERMS_READ_WRITE_TRIGGER},
    {CID_RELAY_P2, STR("RelayP2"), STR("on/off"), MB_DEVICE_ADDR1, MB_PARAM_COIL, 8, 8,
     COIL_OFFSET(coils_port1), PARAM_TYPE_U16, 2, OPTS(BIT0, 0, 0), PAR_PERMS_READ_WRITE_TRIGGER}};

// Calculate number of parameters in the table
const uint16_t num_device_parameters = (sizeof(device_parameters) / sizeof(device_parameters[0]));

// The function to get pointer to parameter storage (instance) according to parameter description table
static void *master_get_param_data(const mb_parameter_descriptor_t *param_descriptor)
{
    assert(param_descriptor != NULL);
    void *instance_ptr = NULL;
    if (param_descriptor->param_offset != 0)
    {
        switch (param_descriptor->mb_param_type)
        {
        case MB_PARAM_HOLDING:
            instance_ptr = ((void *)&holding_reg_params + param_descriptor->param_offset - 1);
            break;
        case MB_PARAM_INPUT:
            instance_ptr = ((void *)&input_reg_params + param_descriptor->param_offset - 1);
            break;
        case MB_PARAM_COIL:
            instance_ptr = ((void *)&coil_reg_params + param_descriptor->param_offset - 1);
            break;
        case MB_PARAM_DISCRETE:
            instance_ptr = ((void *)&discrete_reg_params + param_descriptor->param_offset - 1);
            break;
        default:
            instance_ptr = NULL;
            break;
        }
    }
    else
    {
        ESP_LOGE(TAG, "Wrong parameter offset for CID #%d", param_descriptor->cid);
        assert(instance_ptr != NULL);
    }
    return instance_ptr;
}


// Modbus master initialization
static esp_err_t master_init(void)
{
    // Initialize and start Modbus controller
    mb_communication_info_t comm = {
        .port = MB_PORT_NUM,
#if CONFIG_MB_COMM_MODE_ASCII
        .mode = MB_MODE_ASCII,
#elif CONFIG_MB_COMM_MODE_RTU
        .mode = MB_MODE_RTU,
#endif
        .baudrate = MB_DEV_SPEED,
        .parity = MB_PARITY_NONE
    };
    void *master_handler = NULL;

    esp_err_t err = mbc_master_init(MB_PORT_SERIAL_MASTER, &master_handler);
    MB_RETURN_ON_FALSE((master_handler != NULL), ESP_ERR_INVALID_STATE, TAG,
                       "mb controller initialization fail.");
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb controller initialization fail, returns(0x%x).",
                       (uint32_t)err);
    err = mbc_master_setup((void *)&comm);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb controller setup fail, returns(0x%x).",
                       (uint32_t)err);

    // Set UART pin numbers
    err = uart_set_pin(MB_PORT_NUM, CONFIG_MB_UART_TXD, CONFIG_MB_UART_RXD,
                       CONFIG_MB_UART_RTS, UART_PIN_NO_CHANGE);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb serial set pin failure, uart_set_pin() returned (0x%x).", (uint32_t)err);

    err = mbc_master_start();
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb controller start fail, returns(0x%x).",
                       (uint32_t)err);

    // Set driver mode to Half Duplex
    err = uart_set_mode(MB_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb serial set mode failure, uart_set_mode() returned (0x%x).", (uint32_t)err);

    vTaskDelay(5);
    err = mbc_master_set_descriptor(&device_parameters[0], num_device_parameters);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb controller set descriptor fail, returns(0x%x).",
                       (uint32_t)err);
    ESP_LOGI(TAG, "Modbus master stack initialized...");
    return err;
}


void run_mbus_master(void)
{
    esp_log_level_set("*", ESP_LOG_VERBOSE);

    // Initialization of device peripheral and objects
    ESP_ERROR_CHECK(master_init());
    vTaskDelay(10);

    // Example: Query holding registers 0 to 999
    // query_registers_range(0, 999, 0x03); // For holding registers
    //  query_registers_range(0, 999, 0x04); // For input registers, uncomment if needed

    // Start UART logging task
    xTaskCreate(uart_log_task, "uart_log_task", 2048, NULL, 5, NULL);

    //xTaskCreate(modbus_task, "modbus_task", 4096, NULL, 10, NULL);

    // log_raw_uart_data();

    //master_operation_func(NULL);
}
