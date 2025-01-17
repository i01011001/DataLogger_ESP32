#ifndef CONNECT_H
#define CONNECT_H

#include <esp_err.h>

/* void connect_init(void); */
/* esp_err_t connect_sta(char* SSID, char* PASS, int TIMEOUT); */
esp_err_t wifi_init();

#endif
