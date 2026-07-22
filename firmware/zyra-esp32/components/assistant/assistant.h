#pragma once

#include "esp_err.h"
#include "oled.h"
#include "speaker.h"
#include "api.h"

typedef struct {
    char base_url[128];
    oled_display_t* display;
    speaker_t* speaker;
} assistant_t;

// Initializes the assistant client settings
void assistant_init(assistant_t* client, oled_display_t* display, speaker_t* speaker, const char* base_url);

// Sends recording buffer over HTTP client POST, parses response JSON metadata, prints to OLED, and streams back reply audio
esp_err_t assistant_send_voice_chat(assistant_t* client, audio_buffer_t* audio_buf);
