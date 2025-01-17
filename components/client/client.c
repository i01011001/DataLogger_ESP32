
#include "freertos/projdefs.h"
#include <cJSON.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <stdio.h>
#include <string.h>

static char buffer_middle_man[512] = {};
static char buffer[512] = {};
static int buffer_index = 0;
cJSON *time_buffer;

#define TAG "http"
char rtc_time[20];

// for getting the data from HTTPS
esp_err_t client_event(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        // ESP_LOGI("DATA_SIZE", "%d", evt->data_len);
        snprintf(buffer_middle_man, evt->data_len, "%s", (char *)evt->data);
        if (buffer_middle_man[0] == '{') {
            buffer_index = 0;
        }
        for (int i = 0; i < evt->data_len; i++) {
            buffer[buffer_index + i] = buffer_middle_man[i];
        }
        if (buffer_index != 0) {
            buffer[buffer_index + evt->data_len - 1] = '}';
        }
        buffer_index = evt->data_len - 1;
        break;
    default:
        break;
    }
    return ESP_OK;
}

char *rtc_client_setup() {
    esp_http_client_config_t client_config = {
        .url = "http://worldtimeapi.org/api/timezone/asia/kathmandu",
        .method = HTTP_METHOD_GET,
        .event_handler = client_event,
    };
    esp_http_client_handle_t client_handle = esp_http_client_init(&client_config);
    esp_http_client_set_header(client_handle, "Content_type", "application/json");
    esp_http_client_perform(client_handle);
    time_buffer = cJSON_Parse(buffer);
    esp_http_client_cleanup(client_handle);
    if (time_buffer == NULL) {
        ESP_LOGE("RTC", "The value is NULL");
        return (0);
    } else {
        cJSON *datetime = cJSON_GetObjectItem(time_buffer, "datetime");
        if (datetime && cJSON_IsString(datetime)) {
            ESP_LOGI("TIME", " %s\n\n", datetime->valuestring);
            snprintf(rtc_time, 20, "%s", datetime->valuestring);
            if (time_buffer) {
                cJSON_Delete(time_buffer);
            }
            return (rtc_time);
        }
        return 0;
    }
}
