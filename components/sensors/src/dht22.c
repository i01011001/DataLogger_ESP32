#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <dht.h>

#define SENSOR_TYPE DHT_TYPE_AM2301
#define DATA_PIN 27

float temperature, humidity = 0;

static void dht_test(void *pvParameters);

float get_humidity() { return humidity; }

float get_temperature() { return temperature; }

void dht22_init()
{
    xTaskCreatePinnedToCore(dht_test, "dht_test", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL,1);
}

static void dht_test(void *pvParameters)
{
    gpio_set_pull_mode(DATA_PIN, GPIO_PULLUP_ONLY);

    while (1)
    {
        if (dht_read_float_data(1, DATA_PIN, &humidity, &temperature) == ESP_OK)
            printf("Humidity: %.1f%% Temp: %.1fC\n", humidity, temperature);
        else
            printf("Could not read data from sensor\n");

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
