#include "freertos/idf_additions.h"
#include <driver/i2c.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <i2c_rw.h>
#include <server.h>

#include <driver/gpio.h>
#include <esp_timer.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <ntp.h>

// -----------------------------[  ]--------------------------------- //

#if HELPER_TARGET_IS_ESP32
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
#define PORT_ENTER_CRITICAL() portENTER_CRITICAL(&mux)
#define PORT_EXIT_CRITICAL() portEXIT_CRITICAL(&mux)

#elif HELPER_TARGET_IS_ESP8266
#define PORT_ENTER_CRITICAL() portENTER_CRITICAL()
#define PORT_EXIT_CRITICAL() portEXIT_CRITICAL()
#endif

#define CHECK_ARG(VAL)                                                                                                                                         \
    do {                                                                                                                                                       \
        if (!(VAL))                                                                                                                                            \
            return ESP_ERR_INVALID_ARG;                                                                                                                        \
    } while (0)

#define CHECK_LOGE(x, msg, ...)                                                                                                                                \
    do {                                                                                                                                                       \
        esp_err_t __;                                                                                                                                          \
        if ((__ = x) != ESP_OK) {                                                                                                                              \
            PORT_EXIT_CRITICAL();                                                                                                                              \
            ESP_LOGE(TAG, msg, ##__VA_ARGS__);                                                                                                                 \
            return __;                                                                                                                                         \
        }                                                                                                                                                      \
    } while (0)

// -----------------------------[ I2C ]--------------------------------- //
#define CHP_SDA1 18
#define CHP_SCL1 19
#define MASTER_PORT1 I2C_NUM_1

#define CHP_SDA0 32
#define CHP_SCL0 33
#define MASTER_PORT0 I2C_NUM_0

// -----------------------------[ RTC ]--------------------------------- //

#define REGISTER_READ_AMOUNT 7
#define RTC_ADDR 0x68

uint8_t char_to_hex(char pos_10, char pos_1) {
    pos_10 -= '0';
    pos_1 -= '0';
    return ((0x0f & pos_10) << 4) | (0x0f & pos_1);
}

void time_sync() {
    uint8_t sec = 0;
    uint8_t min = 0;
    uint8_t hr = 0;
    // uint8_t day=0;
    uint8_t date = 0;
    uint8_t month = 0;
    uint8_t year = 0;

    char time[100] = {};

    while (timeinfo.tm_mday == 0) {
        ESP_LOGI("SENSOR", " getting the time for RTC");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    sprintf(time, "%02d:%02d:%02d %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, timeinfo.tm_year + 1900, timeinfo.tm_mon,
            timeinfo.tm_mday);

    hr = char_to_hex(time[0], time[1]);
    min = char_to_hex(time[3], time[4]);
    sec = char_to_hex(time[6], time[7]);
    // day= char_to_hex(0, time[10]);
    date = char_to_hex(time[17], time[18]);
    month = char_to_hex(time[14], time[15]);
    year = char_to_hex(time[11], time[12]);

    uint8_t data[REGISTER_READ_AMOUNT] = {};

    i2c_init(CHP_SDA1, CHP_SCL1, MASTER_PORT1);
    i2c_write(RTC_ADDR, MASTER_PORT1, 0x07, 0xb3);
    i2c_write(RTC_ADDR, MASTER_PORT1, 0x00, sec);
    i2c_write(RTC_ADDR, MASTER_PORT1, 0x01, min);
    i2c_write(RTC_ADDR, MASTER_PORT1, 0x02, hr);
    // i2c_write(RTC_ADDR, MASTER_PORT1, 0x03, day);
    i2c_write(RTC_ADDR, MASTER_PORT1, 0x04, date);
    i2c_write(RTC_ADDR, MASTER_PORT1, 0x05, month);
    i2c_write(RTC_ADDR, MASTER_PORT1, 0x06, year);

    ESP_LOGI("RTC", "Synced time = %s", time);

    while (1) {
        i2c_read(RTC_ADDR, MASTER_PORT1, 0x00, data);
        for (int i = 0; i < 7; i++) {
            time_data[i] = data[i];
        }
        // printf("%d:%d:%d \n\n",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);

        // printf("sec: %d\n",sec);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void tinyRTC_init() { xTaskCreate(time_sync, "RTC", 4000, NULL, 2, NULL); }

// -----------------------------[ DHT22 ]--------------------------------- //
#define DHT_TIMEOUT_ERROR -2
#define DHT_CHECKSUM_ERROR -1
#define DHT_OK 0

#define DHT 22
#define DHT_NEG 23

#define tag "DHT"

int pos_count = 0;
int neg_count = 0;

int pos_time[44];
int neg_time[44];

bool dht_data[40];
uint8_t data[5];

float humidity;
float temperature;

TaskHandle_t signal_handler;

// function prototype
void DHT_start_signal();
int DHT_process_signal();

float get_humidity() { return humidity; }
float get_temperature() { return temperature; }

static IRAM_ATTR void pos_intr() {
    pos_time[pos_count] = esp_timer_get_time();
    pos_count++;
}

static IRAM_ATTR void neg_intr() {
    neg_time[neg_count] = esp_timer_get_time();
    neg_count++;
}

void error_handler(int response) {
    switch (response) {
    case DHT_TIMEOUT_ERROR:
        ESP_LOGE(tag, "DHT_TIMEOUT_ERROR");
        break;
    case DHT_CHECKSUM_ERROR:
        ESP_LOGE(tag, "DHT_CHECKOUT_ERROR");
        break;
    case DHT_OK:
        // ESP_LOGI("DHT", "Humidity->%4.2f\tTemperature->%4.2f", get_humidity(), get_temperature());
        break;
    default:
        ESP_LOGE(tag, "DHT_UNKNOWN_ERROR");
    }
}

void DHT_start_signal() {
    while (1) {
        gpio_isr_handler_remove(DHT);
        gpio_isr_handler_remove(DHT_NEG);
        gpio_pulldown_en(DHT);
        gpio_set_direction(DHT, GPIO_MODE_OUTPUT);
        gpio_set_direction(DHT_NEG, GPIO_MODE_OUTPUT);
        gpio_set_level(DHT, 0);
        esp_rom_delay_us(1000);
        gpio_set_level(DHT, 1);
        esp_rom_delay_us(25);
        gpio_set_level(DHT, 0);
        gpio_set_direction(DHT, GPIO_MODE_INPUT);
        gpio_set_direction(DHT_NEG, GPIO_MODE_INPUT);
        gpio_isr_handler_add(DHT, pos_intr, NULL);
        gpio_isr_handler_add(DHT_NEG, neg_intr, NULL);
        vTaskDelay(pdMS_TO_TICKS(50));
        xTaskNotifyGive(signal_handler);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// convert the input to tempereture & humidity
int DHT_process_signal() {
    for (int i = 0; i < 41; i++) {
        int utime = neg_time[i] - pos_time[i];
        if (utime > 0) {
            dht_data[i] = utime > 60 ? 1 : 0;
        } else {
            return DHT_TIMEOUT_ERROR;
        }
    }
    memset(data, 0, sizeof(data));
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 8; j++) {
            int k = (i * 8) + j;
            int l = 7 - (k % 8);
            if (dht_data[k + 1])
                data[i] |= (1 << l);
        }
    }
    uint8_t checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
    humidity = ((data[0] * 0x100) + data[1]) / 10.0;
    temperature = ((data[2] * 0x100) + data[3]) / 10.0;
    if (checksum != data[4])
        return DHT_CHECKSUM_ERROR;
    return ESP_OK;
}

void dht_output() {
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        error_handler(DHT_process_signal());
        pos_count = 0;
        neg_count = 0;
    }
}

void intr_init() {
    gpio_install_isr_service(0);

    gpio_config_t pos_pin = {
        .intr_type = GPIO_INTR_POSEDGE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pin_bit_mask = (1ULL << DHT),
    };
    gpio_config(&pos_pin);
    gpio_isr_handler_add(DHT, pos_intr, NULL);

    gpio_config_t neg_pin = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pin_bit_mask = (1ULL << DHT_NEG),
    };

    gpio_config(&neg_pin);
    gpio_isr_handler_add(DHT_NEG, neg_intr, NULL);
}

void dht_init() {
    intr_init();
    xTaskCreatePinnedToCore(DHT_start_signal, "Start Signal", 2048, NULL, 6, NULL, 1);
    xTaskCreatePinnedToCore(dht_output, "output value", 2048, NULL, 3, &signal_handler, 1);
}

// -----------------------------[ MPU6050 ]--------------------------------- //

#define MPU6050_ADDR 0x68

// #define WHO_AM_I 0x75
#define POW_MAG 0x6b

char accel[100] = {};
char gyro[100] = {};

float h2d(uint8_t *data) { return (((data[0] << 8) | data[1]) / 65536.0) * 360.0; }

void mpu6050_conf() {
    uint8_t accel_x[2] = {};
    uint8_t accel_y[2] = {};
    uint8_t accel_z[2] = {};
    uint8_t gyro_x[2] = {};
    uint8_t gyro_y[2] = {};
    uint8_t gyro_z[2] = {};
    // uint8_t temp[2]={};
    // int16_t signed_temp=0;

    i2c_init(CHP_SDA0, CHP_SCL0, MASTER_PORT0);

    // i2c_read(WHO_AM_I, data);
    i2c_write(MPU6050_ADDR, MASTER_PORT0, POW_MAG, 0x04);
    i2c_write(MPU6050_ADDR, MASTER_PORT0, 0x28, 0xf8);
    // i2c_write(0x26, 0x08);
    ESP_LOGI("STATUS", "Wrote to power_mgr");
    while (1) {
        i2c_read(MPU6050_ADDR, MASTER_PORT0, 0x3b, accel_x);
        i2c_read(MPU6050_ADDR, MASTER_PORT0, 0x3d, accel_y);
        i2c_read(MPU6050_ADDR, MASTER_PORT0, 0x3f, accel_z);
        i2c_read(MPU6050_ADDR, MASTER_PORT0, 0x43, gyro_x);
        i2c_read(MPU6050_ADDR, MASTER_PORT0, 0x45, gyro_y);
        i2c_read(MPU6050_ADDR, MASTER_PORT0, 0x47, gyro_z);
        // i2c_read(MPU6050_ADDR, MASTER_PORT0, 0x41, temp);
        // signed_temp= (int16_t)((temp[0]<<8)|temp[1]);

        // printf("accel: %10.2f%10.2f%10.2f\n", h2d(accel_x), h2d(accel_y), h2d(accel_z));
        // printf("gyro: %11.2f%10.2f%10.2f\n", h2d(gyro_x), h2d(gyro_y), h2d(gyro_z));
        // printf("temp: %11.2f\n\n", (signed_temp/340.0)+36.53);

        snprintf(accel, sizeof(accel), "%03.0f | %03.0f | %03.0f", h2d(accel_x), h2d(accel_y), h2d(accel_z));
        snprintf(gyro, sizeof(gyro), "%03.0f | %03.0f | %03.0f", h2d(gyro_x), h2d(gyro_y), h2d(gyro_z));
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}

char *get_accel() { return accel; }

char *get_gyro() { return gyro; }

void mpu6050_init() { xTaskCreate(mpu6050_conf, "MPU6050", 2048, NULL, 2, NULL); }

//-----------------------------[ tsl2561 ]--------------------------------- //

#define TSL2561 0x39
#define MAX_COUNT 2

float lux = 0;

float digital_to_lux(float ch0, float ch1) {
    float value = ch1 / ch0;
    if (0 < value && value <= .52) {
        return (0.0315 * ch0 - 0.0593 * ch0 * pow(value, 1.4));
    } else if (0.52 < value && value <= .65) {
        return (0.0229 * ch0 - 0.0291 * ch1);
    } else if (0.65 < value && value <= .80) {
        return (0.0157 * ch0 - 0.018 * ch1);
    } else if (0.80 < value && value <= 1.3) {
        return (0.00338 * ch0 - 0.0026 * ch1);
    } else if (1.3 < value) {
        return 0;
    } else {
        return -1;
    };
}

void tsl2561_conf() {
    uint8_t data0[MAX_COUNT] = {};
    uint8_t data1[MAX_COUNT] = {};
    uint16_t ch0 = 0;
    uint16_t ch1 = 0;

    // esp_error_check_without_abort(i2c_init(chp_sda0, chp_scl0, master_port0));
    i2c_init(CHP_SDA1, CHP_SCL1, MASTER_PORT1);

    // i2c_write(chp_addr0, master_port0, chp_config0, config_value0);
    i2c_write(TSL2561, MASTER_PORT1, 0x80, 0x03);
    i2c_write(TSL2561, MASTER_PORT1, 0x81, 0x11);

    while (1) {
        i2c_read(TSL2561, MASTER_PORT1, 0x8c, data0);
        i2c_read(TSL2561, MASTER_PORT1, 0x8e, data1);

        ch0 = (data0[1] << 7) + data0[0];
        ch1 = (data1[1] << 7) + data1[0];

        // printf("%8d", ch0);
        // printf("%8d", ch1);

        lux = digital_to_lux(ch0, ch1);
        if (0 > lux) {
            printf("an error has occured");
            ESP_LOGE("tsl2561", "an error has occured");
        }
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}

void tsl2561_init() { xTaskCreate(tsl2561_conf, "tsl2561_conf", 2048, NULL, 2, NULL); }
float get_lux() { return lux; }
