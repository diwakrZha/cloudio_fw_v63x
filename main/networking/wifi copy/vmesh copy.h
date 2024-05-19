// zh_network_handler.h
#ifndef VMESH_H
#define VMESH_H

#include <esp_event.h>


// Function declarations
void zh_network_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void mesh_send_task(void *pvParameters);
void get_current_timestamp(char *buffer, size_t buffer_len);
void vmesh_init();

#endif // VMESH_H
