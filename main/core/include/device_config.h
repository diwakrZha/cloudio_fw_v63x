#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include "soc/soc_caps.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#define DEVICE_ID_PREFIX "cld-"


#define SETTING_BTN9 9    // GPIO 9, Boot GPIO
#define ESP_INTR_FLAG_DEFAULT 0
#define DEBOUNCE_TIME_MS 50
#define LONG_PRESS_TIME_MS 5000
#define RESET_PRESS_TIME_MS 10000



// Activity configurations
#define LDO_CONTROL_GPIO 14
#define LED_STRIP_BLINK_GPIO 8
#define ETH_POWER_GPIO 16


// Battery ADC  read configuration
#define BATT_SENSE_ADC ADC_CHANNEL_3 // Fillio v611 // GPIO 1
#define DEFAULT_VREF 3300 // Default VREF
#define NO_OF_SAMPLES 64  // Number of samples for averaging

#define BAT_ADC_CHANNEL     ADC_CHANNEL_3    // GPIO27 is connected to ADC2 Channel 3
#define BAT_ADC_WIDTH       ADC_BITWIDTH_12  // ADC width of 12 bits
#define BAT_ADC_ATTEN       ADC_ATTEN_DB_2_5  // ADC attenuation of 11 dB (0-3.9V)
#define BAT_SAMPLES         16               // Number of samples for averaging


// Battery read configuration
#define BATT_SENSE_EN_GPIO 14        // Fillio v611
#define BATT_SENSE_GPIO 3            // Fillio v611 // GPIO 1
// Radar configuration
// #define RADAR_EN_GPIO 20       // Fillio v611
// #define BATT_SENSE_ADC ADC_CHANNEL_1 // Fillio v611 // GPIO 1

// RS485 communication configuration
#define RS485_TXD (5) // this should have been reversed, not sure why thats not the case, check HW
#define RS485_RXD (4)
#define RS485_RTS (7)
#define RS485_CTS (UART_PIN_NO_CHANGE)

#define GET_UART_READ_TIMEOUT (60000)   
#define GET_UART_RETRY_INTERVAL (20000) 
#define BUF_SIZE (127)
#define BAUD_RATE (38400)
#define PACKET_READ_TICS (100 / portTICK_PERIOD_MS)
#define RS485_TASK_STACK_SIZE (2048)
#define RS485_TASK_PRIO (10)
#define RS485_UART_PORT (1)
#define RS485_READ_TOUT (3)

// SPI communication configuration
#define ETH_SPI_HOST 1
#define ETH_SPI_SCLK_GPIO 6
#define ETH_SPI_MOSI_GPIO 20
#define ETH_SPI_MISO_GPIO 19
#define ETH_SPI_CLOCK_MHZ 10
#define ETH_SPI_CS0_GPIO 15
#define ETH_SPI_INT0_GPIO 18
#define ETH_SPI_PHY_RST0_GPIO 21
#define ETH_SPI_PHY_ADDR0 1
#define EXT_SPI_CS1_GPIO 2

// I2C communication configuration
#define I2C_SCL 22
#define I2C_SDA 23

// EXT ADC DIO configuration
#define EXT_DIO_ADC_GPIO0 0
#define EXT_DIO_ADC_GPIO1 1

void settings_gpio_setup();

void set_gpio_for_outputs(const gpio_num_t *gpios, size_t num_gpios);
void set_gpios_to_input_pullup(const gpio_num_t *gpios, size_t num_gpios);
void set_and_hold_gpio_outputs_low_pulldown(const gpio_num_t *gpios, size_t num_gpios);
void set_and_hold_gpio_outputs_low(const gpio_num_t *gpios, size_t num_gpios);
void dis_hold_gpio(const gpio_num_t *gpios, size_t num_gpios);
void en_hold_gpio(const gpio_num_t *gpios, size_t num_gpios);
void reset_gpios(const gpio_num_t *gpios, size_t num_gpios);
void set_gpios_to_input_pulldown(const gpio_num_t *gpios, size_t num_gpios);


#endif // DEVICE_CONFIG_H
