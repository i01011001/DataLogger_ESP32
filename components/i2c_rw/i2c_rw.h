#ifndef  I2C_RW_H
#define  I2C_RW_H

#include "esp_err.h"

esp_err_t i2c_read(uint8_t chp_addr, uint8_t port, uint8_t data_addr, uint8_t* data);
esp_err_t i2c_write(uint8_t chp_addr, uint8_t port, uint8_t data_addr, uint8_t data);
esp_err_t i2c_init(uint8_t sda, uint8_t scl, uint8_t port);

#endif
