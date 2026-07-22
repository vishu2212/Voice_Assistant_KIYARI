#pragma once

#include "driver/i2s_std.h"
#include "esp_err.h"
#include <stdbool.h>

typedef struct {
    i2s_port_t port;
    i2s_chan_handle_t tx_handle;
    bool initialized;
    bool running;
} speaker_t;

// Initializes the I2S TX channel for MAX98357A
esp_err_t speaker_init(speaker_t* spk, i2s_port_t port, int bclk_io, int ws_io, int dout_io, int sample_rate);

// Starts DMA playback
esp_err_t speaker_start(speaker_t* spk);

// Stops DMA playback
esp_err_t speaker_stop(speaker_t* spk);

// Writes sample bytes to speaker DMA
esp_err_t speaker_write(speaker_t* spk, const void* src, size_t size, size_t* bytes_written, uint32_t timeout_ms);
