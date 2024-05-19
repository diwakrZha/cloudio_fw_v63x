#include "esp_log.h"

void handle_packed_data(const uint8_t *data, int length) {
    char packed_data[3 * length]; // Assuming each byte will take up to 3 characters (2 for value + 1 for delimiter)
    int packed_len = 0;

    for (int i = 0; i < length; i++) {
        if (i > 0) {
            packed_data[packed_len++] = '#'; // Hash delimiter
        }
        packed_len += sprintf(&packed_data[packed_len], "%d", data[i]); // Pack byte as string
    }
    packed_data[packed_len] = '\0'; // Null-terminate the string

    ESP_LOGI("PACKED_DATA_HANDLER", "Packed data: %s", packed_data);
}
