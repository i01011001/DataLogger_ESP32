#include <driver/i2c.h>

#define MASTER_FREQ 400000
#define MASTER_TIMEOUT 1000
#define REGISTER_READ_AMOUNT 7

void i2c_read(uint8_t chp_addr, uint8_t port, uint8_t data_addr, uint8_t *data) {
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (chp_addr << 1) | I2C_MASTER_WRITE, 1);
    i2c_master_write_byte(cmd_handle, data_addr, 1);
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (chp_addr << 1) | I2C_MASTER_READ, 1);
    i2c_master_read(cmd_handle, data, REGISTER_READ_AMOUNT, I2C_MASTER_ACK);
    i2c_master_stop(cmd_handle);
    i2c_master_cmd_begin(port, cmd_handle, portMAX_DELAY);
    i2c_cmd_link_delete(cmd_handle);
}

void i2c_write(uint8_t chp_addr, uint8_t port, uint8_t data_addr, uint8_t data) {
    i2c_cmd_handle_t cmd_handle_write = i2c_cmd_link_create();
    i2c_master_start(cmd_handle_write);
    i2c_master_write_byte(cmd_handle_write, (chp_addr << 1) | I2C_MASTER_WRITE, 1);
    i2c_master_write_byte(cmd_handle_write, data_addr, 1);
    i2c_master_write_byte(cmd_handle_write, data, 1);
    i2c_master_stop(cmd_handle_write);
    i2c_master_cmd_begin(port, cmd_handle_write, portMAX_DELAY);
    i2c_cmd_link_delete(cmd_handle_write);
}

void i2c_init(uint8_t sda, uint8_t scl, uint8_t port) {
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = scl,
        .sda_io_num = sda,
        .master.clk_speed = 400000,
    };
    i2c_param_config(port, &i2c_conf);
    i2c_driver_install(port, I2C_MODE_MASTER, 0, 0, 0);
};
