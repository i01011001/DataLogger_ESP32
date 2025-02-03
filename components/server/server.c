#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "mdns.h"
#include "dht22.h"
#include "server.h"
#include "cJSON.h"

static const char *TAG = "HTTPD SERVER";

extern const char html[] asm("_binary_index_html_start");

uint8_t time_data[] = {};

struct async_resp_arg {
	httpd_handle_t hd;
	int fd;
};

static esp_err_t uri_home(httpd_req_t *req);
static esp_err_t ws_handler(httpd_req_t *req);
static void send_data(void *arg) ;
static void ws_server_send_messages(void *serverd);

// setup for the home page
static esp_err_t uri_home(httpd_req_t *req) {
	ESP_LOGI(TAG, "uri_handler is starting\n");
	httpd_resp_sendstr(req, html);
	return ESP_OK;
}

static esp_err_t ws_handler(httpd_req_t *req) {
	if (req->method == HTTP_GET) {
		ESP_LOGI(TAG, "Handshake done, the new connection was opened");
		return ESP_OK;
	}
	return ESP_OK;
}

static void send_data(void *arg) {
	struct async_resp_arg *resp_arg = arg;
	httpd_handle_t hd = resp_arg->hd;
	int fd = resp_arg->fd;
	// char response[200];
	cJSON *json = cJSON_CreateObject();
	cJSON_AddNumberToObject(json, "humidity", get_humidity());
	cJSON_AddNumberToObject(json, "temperature", get_temperature());
	// cJSON_AddStringToObject(json, "accel", get_accel());
	// cJSON_AddStringToObject(json, "gyro", get_gyro());
	// cJSON_AddNumberToObject(json, "lux", get_lux());
	// cJSON_AddNumberToObject(json, "second", time_data[0]);
	// cJSON_AddNumberToObject(json, "minute", time_data[1]);
	// cJSON_AddNumberToObject(json, "hour", time_data[2]);
	// cJSON_AddNumberToObject(json, "day", time_data[3]);
	// cJSON_AddNumberToObject(json, "date", time_data[4]);
	// cJSON_AddNumberToObject(json, "month", time_data[5]);
	// cJSON_AddNumberToObject(json, "year", time_data[6]);

	char *response = cJSON_Print(json);

	cJSON_Delete(json);

	httpd_ws_frame_t ws_pkt={};
	// memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
	ws_pkt.payload = (uint8_t *)response;
	ws_pkt.len = strlen(response);
	ws_pkt.type = HTTPD_WS_TYPE_TEXT;

	httpd_ws_send_frame_async(hd, fd, &ws_pkt);
	free(resp_arg);
}

static void ws_server_send_messages(void *serverd) {
    httpd_handle_t server = (httpd_handle_t)serverd;
    bool send_messages = true;
    while (send_messages) {
        vTaskDelay(pdMS_TO_TICKS(10000)); // Fixed delay
        if (server == NULL) { // Check handle directly
            ESP_LOGE(TAG, "Server handle is NULL!");
			   continue;
		}
		int max_clients = 10;

		size_t clients = max_clients;
		int client_fds[max_clients];
		if (httpd_get_client_list(server, &clients, client_fds) == ESP_OK) {
			for (size_t i = 0; i < clients; ++i) {
				int sock = client_fds[i];
				if (httpd_ws_get_fd_info(server, sock) == HTTPD_WS_CLIENT_WEBSOCKET) {
					ESP_LOGI(TAG, "Active client (fd=%d) -> sending async message", sock);
					struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
					resp_arg->hd = server;
					resp_arg->fd = sock;
					if (httpd_queue_work(resp_arg->hd, send_data, resp_arg) != ESP_OK) {
						ESP_LOGE(TAG, "httpd_queue_work failed!");
						send_messages = false;
						break;
					}
				}
			}
		} else {
		vTaskDelay(pdMS_TO_TICKS(500));
			ESP_LOGE(TAG, "httpd_get_client_list failed!");
			return;
		}
	}
}

// setup for the server
void server_init() {
	httpd_handle_t httpd_handler = NULL;
	httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
	httpd_start(&httpd_handler, &httpd_config);
	httpd_uri_t httpd_uri = {
		.uri = "/",
		.method = HTTP_GET,
		.handler = uri_home,
	};
	httpd_register_uri_handler(httpd_handler, &httpd_uri);
	httpd_uri_t ws_uri = {
		.uri = "/ws",
		.method = HTTP_GET,
		.handler = ws_handler,
		.is_websocket = true,
	};
	httpd_register_uri_handler(httpd_handler, &ws_uri);
	xTaskCreate(ws_server_send_messages, "send ws", 6000, httpd_handler,4, NULL);
}

void mdns_service() {
	char *hostname = "void-esp32";
	ESP_ERROR_CHECK(mdns_init());
	ESP_ERROR_CHECK(mdns_hostname_set(hostname));
	ESP_ERROR_CHECK(mdns_instance_name_set("just for learning purpose"));
	ESP_LOGW("MDNS", "hostname: %s", hostname);
}
