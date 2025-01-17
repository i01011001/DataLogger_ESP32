idf_component_register(SRCS "sensors.c"
  INCLUDE_DIRS "."
  REQUIRES
  driver
  i2c_rw
  server
  client
  esp_timer
  ntp
  )


