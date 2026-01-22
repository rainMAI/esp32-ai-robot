idf_component_register(
    SRCS "chat_recorder.cc"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        esp_wifi
        cJSON
)
