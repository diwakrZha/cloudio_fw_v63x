#ifndef SENSORS_H
#define SENSORS_H

#include <stdbool.h>
#include <stdint.h>

// Function declarations
void temperature_sensor_init();
void temperature_sensor_en(bool enable);

char *get_board_temp();
void adc_en(bool enable);
void get_power_volts(float *scaled_voltage, int *adc_reading);

#endif // SENSORS_H
