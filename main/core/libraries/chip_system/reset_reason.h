#ifndef RESET_REASON_H
#define RESET_REASON_H

#include "esp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the reset reason module. This function will read the reset reason
 * from the RTC registers and store it for later retrieval.
 * 
 * This function is automatically called at startup.
 */
void esp_reset_reason_init(void);

/**
 * @brief Get the reset reason of the ESP32.
 * 
 * @return The reset reason as an `esp_reset_reason_t` value.
 */
esp_reset_reason_t esp_reset_reason(void);

/**
 * @brief Set a hint for the reset reason. This can be used to specify a custom
 * reset reason before triggering a software reset.
 * 
 * @param hint The reset reason hint to set.
 */
void esp_reset_reason_set_hint(esp_reset_reason_t hint);

/**
 * @brief Get the reset reason hint that was set before a reset.
 * 
 * @return The reset reason hint, or ESP_RST_UNKNOWN if no hint was set.
 */
esp_reset_reason_t esp_reset_reason_get_hint(void);

#ifdef __cplusplus
}
#endif

#endif // RESET_REASON_H
