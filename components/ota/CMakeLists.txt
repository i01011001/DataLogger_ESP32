idf_component_register(SRCS "ota.c"
	INCLUDE_DIRS "."
	REQUIRES esp_driver_gpio esp_event esp_http_client esp_https_ota esp_partition esp_netif app_update mbedtls
	EMBED_TXTFILES "./pem/google.pem"
)
