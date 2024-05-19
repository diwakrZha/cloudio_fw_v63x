#ifndef GET_MESH_DATA_H
#define GET_MESH_DATA_H

#include "cJSON.h"
#include <stdint.h> // For uint8_t
#include <stdbool.h> // For bool
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*IncomingJsonHandler)(cJSON* json);
extern IncomingJsonHandler g_incomingJsonHandler;
void registerIncomingJsonHandler(IncomingJsonHandler handler);

// Function declarations
void check_and_clear_config(void);

// Global variable declaration
extern bool config_exists;

// Main application entry point
void sense_mesh(void);

#ifdef __cplusplus
}
#endif

#endif // GET_MESH_DATA_H
