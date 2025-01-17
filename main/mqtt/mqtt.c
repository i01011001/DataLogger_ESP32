#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mqtt.h>
#include <mqtt_client.h>
#include <stdio.h>
// #include <dht.h>

#include <sensors.h>
#include <string.h>

static const char *TAG_M = "MQTT_EVENT";

// Function prototypes
static void mqtt_command(esp_mqtt_event_handle_t event);

static void mqtt_event_handler(void *event_handler_arg,
                               esp_event_base_t event_base, int32_t event_id,
                               void *event_data) {
  esp_mqtt_event_handle_t event = event_data;
  esp_mqtt_client_handle_t client = event->client;
  switch (event->event_id) {
  case MQTT_EVENT_BEFORE_CONNECT:
    ESP_LOGI(TAG_M, "MQTT_EVENT_BEFORE_CONNECT");
    break;
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(TAG_M, "MQTT_EVENT_CONNECTED");
    esp_mqtt_client_subscribe(client, "command", 2);
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG_M, "MQTT_EVENT_DISCONNECTED");
    break;
  case MQTT_EVENT_SUBSCRIBED:
    ESP_LOGI(TAG_M, "MQTT_EVENT_SUBCRIBED");
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    ESP_LOGI(TAG_M, "MQTT_EVENT_UNSUBSCRIBED");
    break;
  case MQTT_EVENT_PUBLISHED:
    ESP_LOGI(TAG_M, "MQTT_EVENT_PUBLISHED");
    break;
  case MQTT_EVENT_DATA:
    ESP_LOGI(TAG_M, "MQTT_EVENT_DATA");
    printf("%.*s: ", event->topic_len, event->topic);
    printf("%.*s\n", event->data_len, event->data);
    mqtt_command(event);

    break;
  case MQTT_EVENT_ERROR:
    ESP_LOGE(TAG_M, "MQTT_EVENT_ERROR");
    break;
  default:
    break;
  }
}

static void mqtt_command(esp_mqtt_event_handle_t event) {
  char command[event->data_len + 1];
  snprintf(command, sizeof(command), "%s", event->data);

  if (strcmp(command, "reset") == 0) {
    printf("reseting now");
    esp_restart();
  }
}

static void mqtt_publish(void *args) {
  char data[16]={};
  esp_mqtt_client_handle_t client = ((esp_mqtt_client_handle_t)args);
  while (true) {
    // snprintf(data, sizeof(data), "%.2f\t%.2f\n", get_humidity(), get_temperature());
    // esp_mqtt_client_publish(client, "esp", data, sizeof(data), 1, 0);
	
	memcpy(data, "kl", sizeof("kl"));
    esp_mqtt_client_publish(client, "home/kichen/light", data, sizeof(data), 1, 0);
	memcpy(data, "kt", sizeof("kt"));
    esp_mqtt_client_publish(client, "home/kichen/temp", data, sizeof(data), 1, 0);
	memcpy(data, "ll", sizeof("ll"));
    esp_mqtt_client_publish(client, "home/living/light", data, sizeof(data), 1, 0);
	memcpy(data, "lt", sizeof("lt"));
    esp_mqtt_client_publish(client, "home/living/temp", data, sizeof(data), 1, 0);
	memcpy(data, "bl", sizeof("kl"));
    esp_mqtt_client_publish(client, "home/bedroom/light", data, sizeof(data), 1, 0);
	memcpy(data, "bt", sizeof("kl"));
    esp_mqtt_client_publish(client, "home/bedroom/temp", data, sizeof(data), 1, 0);
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void mqtt_init() {
  esp_mqtt_client_config_t mqtt_cfg = {
      .broker.address.uri = "mqtt://mqtt.eclipseprojects.io",
  };
  esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler,
                                 client);
  esp_mqtt_client_start(client);

  xTaskCreate(mqtt_publish, "publish",2048,(void*)client, 2, NULL);
}
