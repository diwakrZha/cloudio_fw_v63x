#ifndef BUZZER_H
#define BUZZER_H

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_log.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_TIMER LEDC_TIMER_0
#define DUTY_CYCLE (768)

// Function prototypes
void bz_chirp_up(void);
void bz_chirp_down(void);
void bz_beat_down(void);
void bz_beat_up(void);
void disable_bz(void);
void start_buzzer_task(void (*buzzer_function)(void));

#ifdef __cplusplus
}
#endif

#endif // BUZZER_H
