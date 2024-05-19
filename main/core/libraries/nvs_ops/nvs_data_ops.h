
#ifndef _NVS_DATA_OPS_H
#define _NVS_DATA_OPS_H

#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Save the root MAC address to NVS.
 *
 * @param root_mac_str The root MAC address as a string.
 * @return esp_err_t ESP_OK on success, error code on failure.
 */
esp_err_t save_root_mac(const char *root_mac_str);

/**
 * @brief Save the Wi-Fi channel to NVS.
 *
 * @param channel The Wi-Fi channel.
 * @return esp_err_t ESP_OK on success, error code on failure.
 */
esp_err_t save_channel(int channel);

/**
 * @brief Save the Wi-Fi RSSI to NVS.
 *
 * @param rssi The RSSI value.
 * @return esp_err_t ESP_OK on success, error code on failure.
 */
esp_err_t save_rssi(int rssi);

/**
 * @brief Save the AP MAC address to NVS.
 *
 * @param ap_mac The AP MAC address as a string.
 * @return esp_err_t ESP_OK on success, error code on failure.
 */
esp_err_t save_ap_mac(const char *ap_mac);

/**
 * @brief Save the IP address to NVS.
 *
 * @param ip_address The IP address as a string.
 * @return esp_err_t ESP_OK on success, error code on failure.
 */
esp_err_t save_ip_address(const char *ip_address);

/**
 * @brief Retrieve the root MAC address from NVS.
 *
 * @param root_mac Buffer to store the retrieved root MAC address.
 * @return esp_err_t ESP_OK on success, error code on failure.
 */
esp_err_t get_root_mac(uint8_t *root_mac);

/**
 * @brief Retrieve the Wi-Fi channel from NVS.
 *
 * @param channel Pointer to store the retrieved channel.
 * @return esp_err_t ESP_OK on success, error code on failure.
 */
esp_err_t get_channel(int *channel);

/**
 * @brief Retrieve the Wi-Fi RSSI from NVS.
 *
 * @param rssi Pointer to store the retrieved RSSI.
 * @return esp_err_t ESP_OK on success, error code on failure.
 */
esp_err_t get_rssi(int *rssi);

/**
 * @brief Retrieve the AP MAC address from NVS.
 *
 * @param ap_mac Buffer to store the retrieved AP MAC address.
 * @param max_len The maximum length of the buffer.
 * @return esp_err_t ESP_OK on success, error code on failure.
 */
esp_err_t get_ap_mac(char *ap_mac, size_t max_len);

/**
 * @brief Retrieve the IP address from NVS.
 *
 * @param ip_address Buffer to store the retrieved IP address.
 * @param max_len The maximum length of the buffer.
 * @return esp_err_t ESP_OK on success, error code on failure.
 */
esp_err_t get_ip_address(char *ip_address, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // _NVS_DATA_OPS_H
