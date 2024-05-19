// led.h

#ifndef LED_H
#define LED_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void led_initialize(void);
void led_deinitialize(void);
bool is_led_initialized(void);

void led_red_on(void);
void led_green_on(void);
void led_blue_on(void);
void led_off(void);

void led_red_fade(void);
void led_green_fade(void);
void led_blue_fade(void);

#ifdef __cplusplus
}
#endif

#endif // LED_H
