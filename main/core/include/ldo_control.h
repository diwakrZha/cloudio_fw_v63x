#ifndef LDO_CONTROL_H_
#define LDO_CONTROL_H_

#include "driver/gpio.h"

void ldo_init();
void ldo_on();
void ldo_off();
void eth_pwr_off();
void eth_pwr_on();

#endif /* LDO_CONTROL_H */
