idf_component_register(SRCS "client.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                    esp_http_client
                    json)
