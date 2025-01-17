idf_component_register(SRCS "server.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES
                    "web/index.html"
                    REQUIRES
                    esp_http_server
                    mdns
                    json
                    sensors
                )
