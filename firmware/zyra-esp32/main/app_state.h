#pragma once

typedef enum
{
    APP_BOOT = 0,
    APP_WIFI_CONNECTING,
    APP_IDLE,
    APP_LISTENING,
    APP_PROCESSING,
    APP_SPEAKING,
    APP_ERROR

} app_state_t;
