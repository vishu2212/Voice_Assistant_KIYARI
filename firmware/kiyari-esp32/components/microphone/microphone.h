#pragma once

#include "driver/i2s_std.h"
#include "esp_err.h"
#include <stdbool.h>

typedef struct {
    i2s_port_t port;
    i2s_chan_handle_t rx_handle;
    bool initialized;
    bool running;
} microphone_t;

// Initializes the I2S RX channel for INMP441 mic
esp_err_t microphone_init(microphone_t* mic, i2s_port_t port, int bclk_io, int ws_io, int din_io, int sample_rate);

// Starts DMA recording
esp_err_t microphone_start(microphone_t* mic);

// Stops DMA recording
esp_err_t microphone_stop(microphone_t* mic);

// Reads sample bytes from mic DMA
esp_err_t microphone_read(microphone_t* mic, void* dest, size_t size, size_t* bytes_read, uint32_t timeout_ms);
