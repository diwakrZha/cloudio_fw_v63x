#ifndef _CONNECT_SPEC_H
#define _CONNECT_SPEC_H

#include <stdint.h>

uint8_t get_wifi_connection_channel();
char* get_connected_ap_mac(void);   
char* fetch_external_ip(void);
int get_wifi_rssi();

#endif // _CONNECT_SPEC_H
