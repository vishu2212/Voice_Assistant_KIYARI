#include "speaker.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char* TAG = "Speaker";

esp_err_t speaker_init(speaker_t* spk, i2s_port_t port, int bclk_io, int ws_io, int dout_io, int sample_rate) {
    if (!spk) return ESP_ERR_INVALID_ARG;
    if (spk->initialized) {
        ESP_LOGW(TAG, "Speaker already initialized.");
        return ESP_OK;
    }

    spk->port = port;
    spk->tx_handle = NULL;
    spk->initialized = false;
    spk->running = false;

    ESP_LOGI(TAG, "Initializing I2S TX channel for MAX98357A...");

    // 1. Setup channel configuration
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(spk->port, I2S_ROLE_MASTER);
    
    // Allocate TX channel
    esp_err_t ret = i2s_new_channel(&chan_cfg, &spk->tx_handle, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to allocate I2S TX channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // 2. Setup standard config parameters
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)bclk_io,
            .ws = (gpio_num_t)ws_io,
            .dout = (gpio_num_t)dout_io,
            .din = I2S_GPIO_UNUSED,
        },
    };

    // 3. Initialize I2S TX channel standard mode
    ret = i2s_channel_init_std_mode(spk->tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S TX STD mode: %s", esp_err_to_name(ret));
        i2s_del_channel(spk->tx_handle);
        spk->tx_handle = NULL;
        return ret;
    }

    spk->initialized = true;
    ESP_LOGI(TAG, "I2S TX channel initialized successfully (BCLK=%d, WS=%d, DOUT=%d).", bclk_io, ws_io, dout_io);
    return ESP_OK;
}

esp_err_t speaker_start(speaker_t* spk) {
    if (!spk || !spk->initialized) {
        ESP_LOGE(TAG, "Speaker not initialized.");
        return ESP_ERR_INVALID_STATE;
    }
    if (spk->running) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Enabling I2S TX channel...");
    esp_err_t ret = i2s_channel_enable(spk->tx_handle);
    if (ret == ESP_OK) {
        spk->running = true;
    } else {
        ESP_LOGE(TAG, "Failed to enable I2S TX channel: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t speaker_stop(speaker_t* spk) {
    if (!spk || !spk->running) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Disabling I2S TX channel...");
    esp_err_t ret = i2s_channel_disable(spk->tx_handle);
    if (ret == ESP_OK) {
        spk->running = false;
    } else {
        ESP_LOGE(TAG, "Failed to disable I2S TX channel: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t speaker_write(speaker_t* spk, const void* src, size_t size, size_t* bytes_written, uint32_t timeout_ms) {
    if (!spk || !spk->running) {
        ESP_LOGE(TAG, "I2S TX channel not running.");
        return ESP_ERR_INVALID_STATE;
    }

    return i2s_channel_write(spk->tx_handle, src, size, bytes_written, pdMS_TO_TICKS(timeout_ms));
}
