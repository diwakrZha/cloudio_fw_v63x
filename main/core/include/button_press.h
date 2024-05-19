#ifndef BUTTON_PRESS_H
#define BUTTON_PRESS_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the button on GPIO9.
 * 
 * @return
 *    - ESP_OK on success
 *    - ESP_FAIL on failure
 */
esp_err_t button_init(void);

#ifdef __cplusplus
}
#endif

#endif // BUTTON_PRESS_H
