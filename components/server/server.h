#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>

extern uint8_t time_data[7];
void mdns_service();
void server_init();
#endif
