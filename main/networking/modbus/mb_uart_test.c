#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RX_BUF_SIZE 512
#define MB_PORT_NUM 1 // Example UART port number
#define UART_LOG_TAG "UART_LOG"

// Simulating UART read function for demonstration
int uart_read_bytes(int port, uint8_t *data, int size, int timeout) {
    // Simulate UART read here
    return 10; // Simulated data length
}

// Function to pack data and return as a string
char* get_packed_data() {
    uint8_t *data = (uint8_t *)malloc(RX_BUF_SIZE);
    if (!data) {
        printf("Failed to allocate memory for RX buffer\n");
        return NULL;
    }

    char *packed_data = NULL;
    int total_bytes = 0;

    // Run the loop until data exceeds 50 bytes
    while (total_bytes <= 50) {
        const int len = uart_read_bytes(MB_PORT_NUM, data, RX_BUF_SIZE, 20);
        if (len > 0) {
            // Pack data delimited by hashes
            char packed_data_buffer[3 * len]; // Assuming each byte will take up to 3 characters
            int packed_len = 0;
            for (int i = 0; i < len; i++) {
                if (i > 0) {
                    packed_data_buffer[packed_len++] = '#'; // Hash delimiter
                }
                packed_len += sprintf(&packed_data_buffer[packed_len], "%d", data[i]); // Pack byte as string
            }
            packed_data_buffer[packed_len] = '\0'; // Null-terminate the string

            // Concatenate packed data
            if (packed_data == NULL) {
                packed_data = strdup(packed_data_buffer);
            } else {
                packed_data = realloc(packed_data, strlen(packed_data) + strlen(packed_data_buffer) + 1);
                strcat(packed_data, packed_data_buffer);
            }

            total_bytes += len;
        } else {
            break; // No more data available
        }
    }

    free(data);

    return packed_data;
}
