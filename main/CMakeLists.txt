idf_component_register(SRCS "joystick.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_adc driver espressif__esp_tinyusb)
