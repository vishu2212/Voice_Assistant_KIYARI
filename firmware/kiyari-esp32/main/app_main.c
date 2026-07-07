#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "cJSON.h"
#include "config.h"
#include "oled.h"
#include "wifi.h"
#include "storage.h"
#include "button.h"
#include "microphone.h"
#include "speaker.h"
#include "api.h"
#include "app_state.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *TAG = "KIYARI";
static oled_display_t display_device;
static button_t trg_button;
static microphone_t mic_device;
static speaker_t spk_device;
static RingbufHandle_t audio_ring_buf = NULL;

static esp_websocket_client_handle_t ws_client = NULL;
static bool ws_connected = false;
static app_state_t g_state = APP_BOOT;

static void oled_print_wrapped_string(oled_display_t* dev, int page_start, const char* label, const char* str) {
    oled_print_string(dev, 0, page_start, label);
    int current_page = page_start + 1;
    int col = 0;
    char word[32];
    int word_len = 0;
    
    while (*str && current_page < 8) {
        if (*str == ' ' || *str == '\n') {
            if (*str == '\n') {
                current_page++;
                col = 0;
            } else {
                col += 6;
            }
            str++;
            continue;
        }
        
        word_len = 0;
        while (*str && *str != ' ' && *str != '\n' && word_len < (int)sizeof(word) - 1) {
            word[word_len++] = *str++;
        }
        word[word_len] = '\0';
        
        int word_width = word_len * 6;
        if (col + word_width >= 128) {
            current_page++;
            col = 0;
            if (current_page >= 8) break;
        }
        
        for (int i = 0; i < word_len; i++) {
            oled_print_char(dev, col, current_page, word[i]);
            col += 6;
        }
    }
}

static void update_state(app_state_t new_state) {
    g_state = new_state;
    if (!display_device.initialized) return;

    oled_clear(&display_device);
    oled_print_string(&display_device, 0, 1, "KIYARI");

    switch (g_state) {
        case APP_BOOT:
            oled_print_string(&display_device, 0, 3, "Booting...");
            break;
        case APP_WIFI_CONNECTING:
            oled_print_string(&display_device, 0, 3, "Connecting WiFi...");
            break;
        case APP_IDLE:
            oled_print_string(&display_device, 0, 3, "Connected");
            oled_print_string(&display_device, 0, 4, "Ready");
            oled_print_string(&display_device, 0, 6, "Press BOOT to speak");
            break;
        case APP_LISTENING:
            oled_print_string(&display_device, 0, 3, "Listening...");
            break;
        case APP_PROCESSING:
            oled_print_string(&display_device, 0, 3, "Thinking...");
            break;
        case APP_SPEAKING:
            oled_print_string(&display_device, 0, 3, "Speaking...");
            break;
        case APP_ERROR:
            oled_print_string(&display_device, 0, 3, "Error occurred");
            break;
    }
    oled_refresh(&display_device);
}

static void audio_playback_task(void *pvParameters) {
    while (true) {
        if (audio_ring_buf == NULL) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        size_t item_size = 0;
        uint8_t *item = (uint8_t *)xRingbufferReceiveUpTo(audio_ring_buf, &item_size, pdMS_TO_TICKS(50), 2048);
        if (item != NULL) {
            size_t bytes_written = 0;
            speaker_write(&spk_device, item, item_size, &bytes_written, 1000);
            vRingbufferReturnItem(audio_ring_buf, (void *)item);
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

static void play_beep(speaker_t* dev, float freq_hz, int duration_ms) {
    int num_samples = (SAMPLE_RATE * duration_ms) / 1000;
    int16_t* buffer = (int16_t*)malloc(num_samples * sizeof(int16_t));
    if (buffer) {
        for (int i = 0; i < num_samples; i++) {
            double t = (double)i / SAMPLE_RATE;
            buffer[i] = (int16_t)(6000.0 * sin(2.0 * M_PI * freq_hz * t));
        }
        size_t bytes_written = 0;
        speaker_start(dev);
        speaker_write(dev, (uint8_t*)buffer, num_samples * sizeof(int16_t), &bytes_written, 1000);
        speaker_stop(dev);
        free(buffer);
    }
}

static void play_wake_chime(void) {
    play_beep(&spk_device, 880.0, 60);
    vTaskDelay(pdMS_TO_TICKS(10));
    play_beep(&spk_device, 1100.0, 80);
}

static void audio_record_task(void *pvParameters) {
    size_t chunk_size = 512;
    int32_t* rx_buffer = (int32_t*)malloc(chunk_size * sizeof(int32_t));
    int16_t* pcm16_buffer = (int16_t*)malloc(chunk_size * sizeof(int16_t));
    
    if (rx_buffer == NULL || pcm16_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate audio recording buffers!");
        vTaskDelete(NULL);
        return;
    }
    
    microphone_start(&mic_device);
    
    while (true) {
        if (ws_connected && ws_client && (g_state == APP_IDLE || g_state == APP_LISTENING)) {
            size_t bytes_read = 0;
            esp_err_t err = microphone_read(&mic_device, rx_buffer, chunk_size * sizeof(int32_t), &bytes_read, 100);
            if (err == ESP_OK && bytes_read > 0) {
                size_t num_samples_read = bytes_read / sizeof(int32_t);
                for (size_t i = 0; i < num_samples_read; i++) {
                    pcm16_buffer[i] = (int16_t)(rx_buffer[i] >> 14);
                }
                esp_websocket_client_send_bin(ws_client, (const char*)pcm16_buffer, num_samples_read * sizeof(int16_t), pdMS_TO_TICKS(100));
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket Connected");
            ws_connected = true;
            update_state(APP_IDLE);
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WebSocket Disconnected");
            ws_connected = false;
            update_state(APP_ERROR);
            break;
        case WEBSOCKET_EVENT_DATA:
            if (data->op_code == 0x01) { // Text
                char *text_data = malloc(data->data_len + 1);
                if (text_data) {
                    memcpy(text_data, data->data_ptr, data->data_len);
                    text_data[data->data_len] = '\0';
                    ESP_LOGI(TAG, "Received WS Text: %s", text_data);
                    
                    cJSON *root = cJSON_Parse(text_data);
                    if (root) {
                        cJSON *event_item = cJSON_GetObjectItem(root, "event");
                        if (event_item && event_item->valuestring) {
                            if (strcmp(event_item->valuestring, "speaking") == 0) {
                                cJSON *resp_item = cJSON_GetObjectItem(root, "response");
                                update_state(APP_SPEAKING);
                                if (resp_item && resp_item->valuestring) {
                                    oled_clear(&display_device);
                                    oled_print_string(&display_device, 0, 1, "KIYARI");
                                    oled_print_wrapped_string(&display_device, 3, "Reply:", resp_item->valuestring);
                                    oled_refresh(&display_device);
                                }
                                speaker_start(&spk_device);
                            } else if (strcmp(event_item->valuestring, "done") == 0) {
                                // Wait for the ring buffer to be empty before stopping the speaker
                                if (audio_ring_buf) {
                                    while (xRingbufferGetCurFreeSize(audio_ring_buf) < (32 * 1024 - 128)) {
                                        vTaskDelay(pdMS_TO_TICKS(20));
                                    }
                                }
                                speaker_stop(&spk_device);
                                update_state(APP_IDLE);
                            } else if (strcmp(event_item->valuestring, "thinking") == 0) {
                                update_state(APP_PROCESSING);
                            } else if (strcmp(event_item->valuestring, "listening") == 0) {
                                update_state(APP_LISTENING);
                            }
                        }
                        cJSON_Delete(root);
                    }
                    free(text_data);
                }
            } else if (data->op_code == 0x02) { // Binary PCM
                if (audio_ring_buf) {
                    xRingbufferSend(audio_ring_buf, data->data_ptr, data->data_len, pdMS_TO_TICKS(100));
                }
            }
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WebSocket Event Error");
            break;
    }
}

static void websocket_init(void) {
    esp_websocket_client_config_t ws_cfg = {
        .uri = BACKEND_WS_URL,
    };
    ESP_LOGI(TAG, "Connecting to WebSocket: %s", BACKEND_WS_URL);
    ws_client = esp_websocket_client_init(&ws_cfg);
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, NULL);
    esp_websocket_client_start(ws_client);
}

static void play_tone_440hz(speaker_t* spk, int duration_ms) {
    int num_samples = (SAMPLE_RATE * duration_ms) / 1000;
    int16_t* tone_buffer = (int16_t*)malloc(num_samples * sizeof(int16_t));
    if (!tone_buffer) return;

    for (int i = 0; i < num_samples; i++) {
        double t = (double)i / SAMPLE_RATE;
        tone_buffer[i] = (int16_t)(6000.0 * sin(2.0 * M_PI * 440.0 * t));
    }

    ESP_LOGI(TAG, "Playing 440 Hz test tone for %d ms...", duration_ms);
    speaker_start(spk);
    size_t bytes_written = 0;
    speaker_write(spk, tone_buffer, num_samples * sizeof(int16_t), &bytes_written, 2000);
    speaker_stop(spk);

    free(tone_buffer);
}

void app_main(void)
{
    ESP_LOGI(TAG, "================================");
    ESP_LOGI(TAG, "      KIYARI v%s", KIYARI_VERSION);
    ESP_LOGI(TAG, "================================");
    ESP_LOGI(TAG, "System Booting...");

    // 1. Initialize NVS Storage
    esp_err_t ret = storage_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS Flash Init Failed: %s", esp_err_to_name(ret));
    }

    // 2. Initialize OLED Display
    ret = oled_init(&display_device, OLED_SDA_GPIO, OLED_SCL_GPIO, OLED_I2C_ADDR);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SH1106 OLED Init OK!");
        update_state(APP_BOOT);
    } else {
        ESP_LOGE(TAG, "SH1106 OLED Init Failed: %s", esp_err_to_name(ret));
    }

    // 3. Initialize Wi-Fi
    ret = wifi_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi Manager Init Failed: %s", esp_err_to_name(ret));
    }

    // 4. Connect to Wi-Fi
    update_state(APP_WIFI_CONNECTING);
    ret = wifi_manager_connect(WIFI_SSID, WIFI_PASS, 15000);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi Connected successfully!");
        char ip_address[16] = {0};
        wifi_manager_get_ip(ip_address, sizeof(ip_address));

        if (display_device.initialized) {
            oled_clear(&display_device);
            oled_print_string(&display_device, 0, 1, "KIYARI");
            oled_print_string(&display_device, 0, 3, "Connected");
            oled_print_string(&display_device, 0, 4, ip_address);
            oled_refresh(&display_device);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    } else {
        ESP_LOGE(TAG, "Wi-Fi Connection Failed: %s", esp_err_to_name(ret));
        update_state(APP_ERROR);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    // 5. Initialize Hardware Drivers
    button_init(&trg_button, BOOT_BUTTON_PIN);
    ESP_ERROR_CHECK(microphone_init(&mic_device, I2S_NUM_0, MIC_SCK_GPIO, MIC_WS_GPIO, MIC_SD_GPIO, SAMPLE_RATE));
    ESP_ERROR_CHECK(speaker_init(&spk_device, I2S_NUM_1, SPK_BCLK_GPIO, SPK_LRC_GPIO, SPK_DOUT_GPIO, SAMPLE_RATE));

    // Create RingBuffer for smooth audio playback
    audio_ring_buf = xRingbufferCreate(32 * 1024, RINGBUF_TYPE_BYTEBUF);
    if (audio_ring_buf == NULL) {
        ESP_LOGE(TAG, "Failed to create audio RingBuffer!");
    } else {
        ESP_LOGI(TAG, "Audio RingBuffer created successfully.");
    }

    // Create high-priority audio playback task pinned to Core 1
    xTaskCreatePinnedToCore(audio_playback_task, "audio_playback", 4096, NULL, 5, NULL, 1);

    // 6. Test Speaker Tone Playback (440Hz for 500ms)
    if (display_device.initialized) {
        oled_clear(&display_device);
        oled_print_string(&display_device, 0, 1, "KIYARI");
        oled_print_string(&display_device, 0, 3, "Testing Speaker...");
        oled_refresh(&display_device);
    }
    play_tone_440hz(&spk_device, 500);

    // 7. Initialize WebSocket connection
    if (wifi_manager_is_connected()) {
        websocket_init();
    }

    ESP_LOGI(TAG, "KIYARI initialization complete. Ready for triggers.");

    while (true)
    {
        // Wait for system to be connected and idle
        while (g_state == APP_BOOT || g_state == APP_WIFI_CONNECTING || g_state == APP_ERROR) {
            if (g_state == APP_ERROR && wifi_manager_is_connected() && !ws_connected) {
                // Try to reconnect websocket - destroy old client first
                ESP_LOGI(TAG, "Attempting to reconnect WebSocket...");
                if (ws_client) {
                    esp_websocket_client_stop(ws_client);
                    esp_websocket_client_destroy(ws_client);
                    ws_client = NULL;
                }
                websocket_init();
            }
            vTaskDelay(pdMS_TO_TICKS(5000));
        }

        // Wait until button is pressed (active-low GPIO 0)
        while (!button_is_pressed(&trg_button)) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        ESP_LOGI(TAG, "Button pressed! Starting audio streaming...");
        
        // 1. Send start event
        if (ws_connected && ws_client) {
            const char* start_msg = "{\"event\": \"start\"}";
            esp_websocket_client_send_text(ws_client, start_msg, strlen(start_msg), pdMS_TO_TICKS(1000));
        }
        update_state(APP_LISTENING);

        // Start recording
        microphone_start(&mic_device);

        size_t max_samples = SAMPLE_RATE * 8; // Max 8 seconds recording
        size_t chunk_size = 512;
        int32_t* rx_buffer = (int32_t*)malloc(chunk_size * sizeof(int32_t));
        int16_t* pcm16_buffer = (int16_t*)malloc(chunk_size * sizeof(int16_t));
        
        size_t samples_recorded = 0;
        
        // Loop as long as button is held down
        while (button_is_pressed(&trg_button) && samples_recorded < max_samples) {
            size_t bytes_read = 0;
            esp_err_t err = microphone_read(&mic_device, rx_buffer, chunk_size * sizeof(int32_t), &bytes_read, 100);
            if (err == ESP_OK && bytes_read > 0) {
                size_t num_samples_read = bytes_read / sizeof(int32_t);
                for (size_t i = 0; i < num_samples_read; i++) {
                    // Convert 24-bit aligned value inside 32-bit word to 16-bit signed integer
                    pcm16_buffer[i] = (int16_t)(rx_buffer[i] >> 14);
                }
                
                // Send binary frame
                if (ws_connected && ws_client) {
                    esp_websocket_client_send_bin(ws_client, (const char*)pcm16_buffer, num_samples_read * sizeof(int16_t), pdMS_TO_TICKS(1000));
                }
                samples_recorded += num_samples_read;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        free(rx_buffer);
        free(pcm16_buffer);
        microphone_stop(&mic_device);
        ESP_LOGI(TAG, "Recording finished. Sent samples: %d", samples_recorded);

        // 2. Send stop event
        if (ws_connected && ws_client) {
            const char* stop_msg = "{\"event\": \"stop\"}";
            esp_websocket_client_send_text(ws_client, stop_msg, strlen(stop_msg), pdMS_TO_TICKS(1000));
        }
        update_state(APP_PROCESSING);

        // 3. Wait until server finishes speaking and transitions back to IDLE
        while (g_state != APP_IDLE && g_state != APP_ERROR) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
