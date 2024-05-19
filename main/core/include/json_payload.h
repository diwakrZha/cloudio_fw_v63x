#ifndef JSON_PAYLOAD_H_
#define JSON_PAYLOAD_H_

#include <stdint.h>
#include <stdbool.h>

void prepareJsonPayload(char *payloadBuf, uint32_t ulValueToNotify, bool *isValidData);
void prepReportJsonPayload(char *payloadBuf, uint32_t ulValueToNotify);
void create_report_settings(char **settings_payload);
void wifi_rst_reinit_JsonPayload(char *payloadBuf, uint32_t ulValueToNotify);

#endif // JSON_PAYLOAD_H_
