#ifndef  SENSORS_H
#define  SENSORS_H
#include <freertos/FreeRTOS.h>

void tinyRTC_init();
void dht_init();
float get_humidity();
float get_temperature();
char* get_accel();
char* get_gyro();
void mpu6050_init();
void tsl2561_init();
float get_lux();

#endif
