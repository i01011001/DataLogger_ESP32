#include "esp_err.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include <esp_log.h>
#include <stdio.h>
#include <time.h>

#include <driver/i2c.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <i2c_rw.h>

#define TAG "INDEX"

char Current_Date_Time[100];

struct tm timeinfo;

void Get_current_date_time(char *date_time) {
    // char strftime_buf[64];
    time_t now;
    time(&now);

    setenv("TZ", "UTC-05:45", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    // strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    // strcpy(date_time,strftime_buf);
}

static void initialize_sntp(void) {
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    // #ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
    //   sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    // #endif
    esp_sntp_init();
}

// void set_time(){
void ntp_init(void) {
    int retry = 0;
    const int retry_count = 4;

    initialize_sntp();
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_init();
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    if (retry >= retry_count)
        esp_restart();
    Get_current_date_time(Current_Date_Time);
}
