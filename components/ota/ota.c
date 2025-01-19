#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define intr_button 0
#define TAG "OTA"

#define BUFFER_SIZE 1024
#define OTA_RECV_TIMEOUT 5000

SemaphoreHandle_t sema_handler;

extern const uint8_t cert_pem[] asm("_binary_google_pem_start");

static char ota_write_data[BUFFER_SIZE + 1] = {0};

static void intr_button_pushed();
static void __attribute__((noreturn)) task_fatal_error(void);
static void http_cleanup(esp_http_client_handle_t client);
static void infinite_loop(void);
static void ota_setup(void *pvParameter);
void ota_start(void);

static void intr_setup() {
	gpio_config_t intr_button_config = {
		.intr_type = GPIO_INTR_POSEDGE,
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.pin_bit_mask = (1ULL << intr_button),
	};
	gpio_config(&intr_button_config);
	gpio_install_isr_service(0);
	gpio_isr_handler_add(intr_button, intr_button_pushed, NULL);
}

static void intr_button_pushed() { 
	xSemaphoreGive(sema_handler); 
}

static void __attribute__((noreturn)) task_fatal_error(void)
{
	ESP_LOGE(TAG, "Exiting task due to fatal error...");
	(void)vTaskDelete(NULL);
	while (1) {
		;
	}
}

static void http_cleanup(esp_http_client_handle_t client)
{
	esp_http_client_close(client);
	esp_http_client_cleanup(client);
}

static void infinite_loop(void)
{
	int i = 0;
	ESP_LOGI(TAG, "When a new firmware is available on the server, press the reset button to download it");
	while(1) {
		ESP_LOGI(TAG, "Waiting for a new firmware ... %d", ++i);
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
}

static void ota_setup(void *pvParameter)
{
	esp_err_t err;
	/* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
	esp_ota_handle_t update_handle = 0 ;
	const esp_partition_t *update_partition = NULL;

	ESP_LOGI(TAG, "Starting OTA example task");

	const esp_partition_t *configured = esp_ota_get_boot_partition();
	const esp_partition_t *running = esp_ota_get_running_partition();

	if (configured != running) {
		ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08"PRIx32", but running from offset 0x%08"PRIx32,
				configured->address, running->address);
		ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
	}
	ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08"PRIx32")",
			running->type, running->subtype, running->address);

	esp_http_client_config_t config = {
		// .url = CONFIG_EXAMPLE_FIRMWARE_UPG_URL,
		.url = "https://drive.usercontent.google.com/download?id=14hlL6p5KSGJnfpqwTXCJTugrlqKS0UPr&export=download&authuser=0&confirm=t&uuid=a4b33121-f52e-43b6-bf20-6fcfb10ad95f&at=APvzH3o6qbkUqBf72eNBU4l63RRr:1736242908248",
		.cert_pem = (char *)cert_pem,
		.timeout_ms = OTA_RECV_TIMEOUT,
		.keep_alive_enable = true,
	};

#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL_FROM_STDIN
	char url_buf[OTA_URL_SIZE];
	if (strcmp(config.url, "FROM_STDIN") == 0) {
		example_configure_stdin_stdout();
		fgets(url_buf, OTA_URL_SIZE, stdin);
		int len = strlen(url_buf);
		url_buf[len - 1] = '\0';
		config.url = url_buf;
	} else {
		ESP_LOGE(TAG, "Configuration mismatch: wrong firmware upgrade image url");
		abort();
	}
#endif

#ifdef CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
	config.skip_cert_common_name_check = true;
#endif

	esp_http_client_handle_t client = esp_http_client_init(&config);
	if (client == NULL) {
		ESP_LOGE(TAG, "Failed to initialise HTTP connection");
		task_fatal_error();
	}
	err = esp_http_client_open(client, 0);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
		esp_http_client_cleanup(client);
		task_fatal_error();
	}
	esp_http_client_fetch_headers(client);

	update_partition = esp_ota_get_next_update_partition(NULL);
	assert(update_partition != NULL);
	ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%"PRIx32,
			update_partition->subtype, update_partition->address);

	int binary_file_length = 0;
	/*deal with all receive packet*/
	bool image_header_was_checked = false;
	while (1) {
		int data_read = esp_http_client_read(client, ota_write_data, BUFFER_SIZE);
		if (data_read < 0) {
			ESP_LOGE(TAG, "Error: SSL data read error");
			http_cleanup(client);
			task_fatal_error();
		} else if (data_read > 0) {
			if (image_header_was_checked == false) {
				esp_app_desc_t new_app_info;
				if (data_read > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
					// check current version with downloading
					memcpy(&new_app_info, &ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
					ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);

					esp_app_desc_t running_app_info;
					if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
						ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
					}

					const esp_partition_t* last_invalid_app = esp_ota_get_last_invalid_partition();
					esp_app_desc_t invalid_app_info;
					if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK) {
						ESP_LOGI(TAG, "Last invalid firmware version: %s", invalid_app_info.version);
					}

					// check current version with last invalid partition
					if (last_invalid_app != NULL) {
						if (memcmp(invalid_app_info.version, new_app_info.version, sizeof(new_app_info.version)) == 0) {
							ESP_LOGW(TAG, "New version is the same as invalid version.");
							ESP_LOGW(TAG, "Previously, there was an attempt to launch the firmware with %s version, but it failed.", invalid_app_info.version);
							ESP_LOGW(TAG, "The firmware has been rolled back to the previous version.");
							http_cleanup(client);
							infinite_loop();
						}
					}
#ifndef CONFIG_EXAMPLE_SKIP_VERSION_CHECK
					if (memcmp(new_app_info.version, running_app_info.version, sizeof(new_app_info.version)) == 0) {
						ESP_LOGW(TAG, "Current running version %s is the same as a new %s. We will not continue the update.", running_app_info.version, new_app_info.version);
						http_cleanup(client);
						infinite_loop();
					}
#endif

					image_header_was_checked = true;

					err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
					if (err != ESP_OK) {
						ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
						http_cleanup(client);
						esp_ota_abort(update_handle);
						task_fatal_error();
					}
					ESP_LOGI(TAG, "esp_ota_begin succeeded");
				} else {
					ESP_LOGE(TAG, "received package is not fit len");
					http_cleanup(client);
					esp_ota_abort(update_handle);
					task_fatal_error();
				}
			}
			err = esp_ota_write( update_handle, (const void *)ota_write_data, data_read);
			if (err != ESP_OK) {
				http_cleanup(client);
				esp_ota_abort(update_handle);
				task_fatal_error();
			}
			binary_file_length += data_read;
			ESP_LOGD(TAG, "Written image length %d", binary_file_length);
		} else if (data_read == 0) {
			/*
			 * As esp_http_client_read never returns negative error code, we rely on
			 * `errno` to check for underlying transport connectivity closure if any
			 */
			if (errno == ECONNRESET || errno == ENOTCONN) {
				ESP_LOGE(TAG, "Connection closed, errno = %d", errno);
				break;
			}
			if (esp_http_client_is_complete_data_received(client) == true) {
				ESP_LOGI(TAG, "Connection closed");
				break;
			}
		}
	}
	ESP_LOGI(TAG, "Total Write binary data length: %d", binary_file_length);
	if (esp_http_client_is_complete_data_received(client) != true) {
		ESP_LOGE(TAG, "Error in receiving complete file");
		http_cleanup(client);
		esp_ota_abort(update_handle);
		task_fatal_error();
	}

	err = esp_ota_end(update_handle);
	if (err != ESP_OK) {
		if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
			ESP_LOGE(TAG, "Image validation failed, image is corrupted");
		} else {
			ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
		}
		http_cleanup(client);
		task_fatal_error();
	}

	err = esp_ota_set_boot_partition(update_partition);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
		http_cleanup(client);
		task_fatal_error();
	}
	ESP_LOGI(TAG, "Prepare to restart system!");
	esp_restart();
	return ;
}

void ota_start(void) {
	printf("\n");
	sema_handler = xSemaphoreCreateBinary();
	// configuration for isr push button

	intr_setup();

	xTaskCreate(ota_setup, "function to be executed", 1024 * 8, NULL, 2, NULL);
}
