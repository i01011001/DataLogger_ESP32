#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

// Custom libraries
#include <connect.h>
#include <i2c_rw.h>
#include <mqtt.h>
#include <ntp.h>
#include <sensors.h>
#include <server.h>
#include <ota.h>

void app_main(void) {
    wifi_init();
     // vTaskDelay(pdMS_TO_TICKS(1000)); 
    //
    // tsl2561_init();
    // mpu6050_init();
    // dht_init();
    //
    // ntp_init();
    // tinyRTC_init();


    mqtt_init();
    // mdns_service(); 
	// ota_start();
    server_init();
}
