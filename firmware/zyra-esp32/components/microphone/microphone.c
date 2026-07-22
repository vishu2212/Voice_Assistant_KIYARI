#include "microphone.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char* TAG = "Microphone";

esp_err_t microphone_init(microphone_t* mic, i2s_port_t port, int bclk_io, int ws_io, int din_io, int sample_rate) {
    if (!mic) return ESP_ERR_INVALID_ARG;
    if (mic->initialized) {
        ESP_LOGW(TAG, "Microphone already initialized.");
        return ESP_OK;
    }

    mic->port = port;
    mic->rx_handle = NULL;
    mic->initialized = false;
    mic->running = false;

    ESP_LOGI(TAG, "Initializing I2S RX channel for INMP441...");

    // 1. Setup channel configuration
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(mic->port, I2S_ROLE_MASTER);
    
    // Allocate RX channel
    esp_err_t ret = i2s_new_channel(&chan_cfg, NULL, &mic->rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to allocate I2S RX channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // 2. Setup standard config parameters
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = (i2s_std_slot_config_t) {
            .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
            .ws_width = 32,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = false,
            .big_endian = false,
            .bit_order_lsb = false
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)bclk_io,
            .ws = (gpio_num_t)ws_io,
            .dout = I2S_GPIO_UNUSED,
            .din = (gpio_num_t)din_io,
        },
    };

    // 3. Initialize I2S RX channel standard mode
    ret = i2s_channel_init_std_mode(mic->rx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S STD mode: %s", esp_err_to_name(ret));
        i2s_del_channel(mic->rx_handle);
        mic->rx_handle = NULL;
        return ret;
    }

    mic->initialized = true;
    ESP_LOGI(TAG, "I2S RX channel initialized successfully (BCLK=%d, WS=%d, DIN=%d).", bclk_io, ws_io, din_io);
    return ESP_OK;
}

esp_err_t microphone_start(microphone_t* mic) {
    if (!mic || !mic->initialized) {
        ESP_LOGE(TAG, "Microphone not initialized.");
        return ESP_ERR_INVALID_STATE;
    }
    if (mic->running) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Enabling I2S RX channel...");
    esp_err_t ret = i2s_channel_enable(mic->rx_handle);
    if (ret == ESP_OK) {
        mic->running = true;
    } else {
        ESP_LOGE(TAG, "Failed to enable I2S RX channel: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t microphone_stop(microphone_t* mic) {
    if (!mic || !mic->running) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Disabling I2S RX channel...");
    esp_err_t ret = i2s_channel_disable(mic->rx_handle);
    if (ret == ESP_OK) {
        mic->running = false;
    } else {
        ESP_LOGE(TAG, "Failed to disable I2S RX channel: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t microphone_read(microphone_t* mic, void* dest, size_t size, size_t* bytes_read, uint32_t timeout_ms) {
    if (!mic || !mic->running) {
        ESP_LOGE(TAG, "I2S RX channel not running.");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = i2s_channel_read(mic->rx_handle, dest, size, bytes_read, pdMS_TO_TICKS(timeout_ms));
    // Debug print of samples removed to prevent console flooding
    return ret;
}
