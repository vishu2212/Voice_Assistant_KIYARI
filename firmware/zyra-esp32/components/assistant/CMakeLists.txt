idf_component_register(
    SRCS "assistant.c"
    INCLUDE_DIRS "."
    REQUIRES display speaker api
    PRIV_REQUIRES esp_http_client json
)
