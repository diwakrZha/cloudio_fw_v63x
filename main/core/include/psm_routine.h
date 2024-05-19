#ifndef PSM_ROUTINE_H_
#define PSM_ROUTINE_H_

#include <stdbool.h>
#include "driver/gpio.h"

// Declare the array as extern
extern const gpio_num_t gpios_output[];
extern const gpio_num_t gpios_input[];
extern const gpio_num_t gpios_to_reset[];

extern const size_t num_gpios_output; // Declare the size
extern const size_t num_gpios_input; // Declare the size
extern const size_t num_gpios_to_reset; // Declare the size

void set_sleep_configs(void);
void disable_holds(void);
void reset_gpios_dis_hold(void);
void set_wake_configs(void);



#endif /* PSM_ROUTINE_H */
