idf_component_register(SRCS  "./src/dht22.c"
  INCLUDE_DIRS "." "./include"
  REQUIRES
  driver
  i2c_rw
  server
  client
  esp_timer
  ntp
  dht
  )


