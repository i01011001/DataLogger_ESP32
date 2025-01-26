#include <esp_log.h>
#include <esp_random.h>


// Whether the given script is using encryption or not,
// generally recommended as it increases security (communication with the server is not in clear text anymore),
// it does come with an overhead tough as having an encrypted session requires a lot of memory,
// which might not be avaialable on lower end devices.
#define ENCRYPTED false


#include <Espressif_MQTT_Client.h>
#include <ThingsBoard.h>

// See https://thingsboard.io/docs/getting-started-guides/helloworld/
// to understand how to obtain an access token
constexpr char TOKEN[] = "TBP1dJdl85iYowoejJIo";

// Thingsboard we want to establish a connection too
constexpr char THINGSBOARD_SERVER[] = "demo.thingsboard.io";

// MQTT port used to communicate with the server, 1883 is the default unencrypted MQTT port,
// whereas 8883 would be the default encrypted SSL MQTT port
constexpr uint16_t THINGSBOARD_PORT = 1883U;

// Maximum size packets will ever be sent or received by the underlying MQTT client,
// if the size is to small messages might not be sent or received messages will be discarded
constexpr uint16_t MAX_MESSAGE_SIZE = 128U;

constexpr char TEMPERATURE_KEY[] = "temperature";


// Initalize the Mqtt client instance
Espressif_MQTT_Client mqttClient;
// Initialize ThingsBoard instance with the maximum needed buffer size
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);

// Status for successfully connecting to the given WiFi
bool wifi_connected = false;


/// @brief Callback method that is called if we got an ip address from the connected WiFi meaning we successfully established a connection
/// @param event_handler_arg User data registered to the event
/// @param event_base Event base for the handler
/// @param event_id The id for the received event
/// @param event_data The data for the event, esp_event_handler_t
void on_got_ip(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    wifi_connected = true;
}

/// @brief Initalizes WiFi connection,
// will endlessly delay until a connection has been successfully established

extern "C" void mqtt_init() {

    for (;;) {

        if (!tb.connected()) {
            tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT);
        }

        tb.sendTelemetryData(TEMPERATURE_KEY, esp_random());
        // tb.sendTelemetryData(HUMIDITY_KEY, esp_random());

        tb.loop();

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
