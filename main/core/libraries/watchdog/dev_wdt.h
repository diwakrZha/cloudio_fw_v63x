// dev_wdt.h

#ifndef DEV_WDT_H
#define DEV_WDT_H

#include "esp_timer.h"

void setup_wdt(int wdt_timeout_sec);
void feed_wdt(void);
void disable_wdt(void);

#endif // DEV_WDT_H
