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

static const char *TAG = "ZYRA";
static oled_display_t display_device;
static button_t trg_button;
static microphone_t mic_device;
static speaker_t spk_device;
static RingbufHandle_t audio_ring_buf = NULL;

static esp_websocket_client_handle_t ws_client = NULL;
static bool ws_connected = false;
static app_state_t g_state = APP_BOOT;

static char speaking_lines[32][24];
static int speaking_line_count = 0;
static int speaking_scroll_index = 0;
static int speaking_scroll_timer = 0;

static void wrap_text_to_lines(const char* str, char lines[32][24], int* line_count) {
    int count = 0;
    int col_chars = 21; // 128 / 6 = 21 chars max per line
    char word[32];
    int word_len = 0;
    char current_line[32] = "";
    int current_len = 0;
    
    *line_count = 0;
    
    while (*str && count < 32) {
        if (*str == ' ' || *str == '\n') {
            if (word_len > 0) {
                // Check if word fits in current line
                if (current_len + (current_len > 0 ? 1 : 0) + word_len <= col_chars) {
                    if (current_len > 0) {
                        strcat(current_line, " ");
                        current_len++;
                    }
                    strcat(current_line, word);
                    current_len += word_len;
                } else {
                    // Push current line
                    if (count < 32) {
                        strcpy(lines[count++], current_line);
                    }
                    strcpy(current_line, word);
                    current_len = word_len;
                }
                word_len = 0;
            }
            if (*str == '\n' && current_len > 0) {
                if (count < 32) {
                    strcpy(lines[count++], current_line);
                }
                current_line[0] = '\0';
                current_len = 0;
            }
            str++;
        } else {
            if (word_len < 31) {
                word[word_len++] = *str;
                word[word_len] = '\0';
            }
            str++;
        }
    }
    
    // Add last word/line if any
    if (word_len > 0) {
        if (current_len + (current_len > 0 ? 1 : 0) + word_len <= col_chars) {
            if (current_len > 0) strcat(current_line, " ");
            strcat(current_line, word);
        } else {
            if (count < 32) strcpy(lines[count++], current_line);
            strcpy(current_line, word);
        }
    }
    if (current_line[0] != '\0' && count < 32) {
        strcpy(lines[count++], current_line);
    }
    
    *line_count = count;
}

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

    // For static states, render immediately
    if (g_state == APP_BOOT || g_state == APP_WIFI_CONNECTING || g_state == APP_IDLE || g_state == APP_ERROR) {
        oled_print_string(&display_device, 0, 1, ZYRA_NAME);
        switch (g_state) {
            case APP_BOOT:
                oled_print_string(&display_device, 0, 3, "Starting " ZYRA_NAME "...");
                break;
            case APP_WIFI_CONNECTING:
                oled_print_string(&display_device, 0, 3, "Connecting WiFi...");
                break;
            case APP_IDLE:
                oled_print_string(&display_device, 0, 3, "Ready.");
                oled_print_string(&display_device, 0, 5, "Say: 'Hey " ZYRA_NAME "'");
                oled_print_string(&display_device, 0, 6, "or press BOOT");
                break;
            case APP_ERROR:
                oled_print_string(&display_device, 0, 3, "Error occurred");
                break;
            default:
                break;
        }
        oled_refresh(&display_device);
    }
}

static void draw_waveform_animation(oled_display_t* dev, int frame) {
    oled_clear(dev);
    oled_print_string(dev, 0, 1, ZYRA_NAME);
    oled_print_string(dev, 0, 3, "Listening...");
    
    // Siri-style flowing waveform
    float phase = frame * 0.3f;
    float max_amp = 6.0f + 3.0f * sinf(frame * 0.15f);
    
    for (int x = 0; x < 128; x++) {
        int y = 48 + (int)(max_amp * sinf((x * 0.12f) - phase));
        if (y >= 0 && y < 64) {
            oled_draw_pixel(dev, x, y, true);
            if (y + 1 < 64) oled_draw_pixel(dev, x, y + 1, true);
        }
    }
}

static void draw_thinking_animation(oled_display_t* dev, int frame) {
    oled_clear(dev);
    oled_print_string(dev, 0, 1, ZYRA_NAME);
    oled_print_string(dev, 0, 3, "Thinking...");
    
    int cx = 64;
    int cy = 48;
    int radius = 8;
    int active_dot = frame % 8;
    
    for (int i = 0; i < 8; i++) {
        float angle = i * (2.0f * 3.14159f / 8.0f);
        int dx = cx + (int)(radius * cosf(angle));
        int dy = cy + (int)(radius * sinf(angle));
        
        if (i == active_dot) {
            for (int xx = -1; xx <= 1; xx++) {
                for (int yy = -1; yy <= 1; yy++) {
                    oled_draw_pixel(dev, dx + xx, dy + yy, true);
                }
            }
        } else {
            oled_draw_pixel(dev, dx, dy, true);
        }
    }
}

static void draw_speaking_animation(oled_display_t* dev, int frame) {
    // Tiny bouncing visualizer equalizer bars in the top right corner (x=95 to 127, y=0 to 15)
    int bar_x[] = {100, 106, 112, 118};
    for (int b = 0; b < 4; b++) {
        int height = 2 + (int)(8.0f * (1.0f + sinf(frame * 0.4f + b * 1.5f)) / 2.0f);
        int x = bar_x[b];
        for (int y = 14; y > 14 - height; y--) {
            oled_draw_pixel(dev, x, y, true);
            oled_draw_pixel(dev, x + 1, y, true);
            oled_draw_pixel(dev, x + 2, y, true);
        }
    }
}

static void display_animation_task(void *pvParameters) {
    int frame = 0;
    while (true) {
        if (g_state == APP_LISTENING) {
            draw_waveform_animation(&display_device, frame);
            oled_refresh(&display_device);
            frame++;
            vTaskDelay(pdMS_TO_TICKS(100));
        } else if (g_state == APP_PROCESSING) {
            draw_thinking_animation(&display_device, frame);
            oled_refresh(&display_device);
            frame++;
            vTaskDelay(pdMS_TO_TICKS(100));
        } else if (g_state == APP_SPEAKING) {
            oled_clear(&display_device);
            oled_print_string(&display_device, 0, 1, ZYRA_NAME);
            
            // Print up to 4 lines of the wrapped response starting from speaking_scroll_index
            int render_page = 3;
            for (int i = speaking_scroll_index; i < speaking_line_count && render_page < 7; i++) {
                oled_print_string(&display_device, 0, render_page, speaking_lines[i]);
                render_page++;
            }
            
            // Draw top-right equalizer animation
            draw_speaking_animation(&display_device, frame);
            oled_refresh(&display_device);
            
            // Scroll logic: scroll every 15 frames (approx 1.5 seconds)
            speaking_scroll_timer++;
            if (speaking_scroll_timer >= 15) {
                speaking_scroll_timer = 0;
                if (speaking_scroll_index + 4 < speaking_line_count) {
                    speaking_scroll_index++;
                }
            }
            
            frame++;
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
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
                    pcm16_buffer[i] = (int16_t)(rx_buffer[i] >> 16);
                }
                esp_websocket_client_send_bin(ws_client, (const char*)pcm16_buffer, num_samples_read * sizeof(int16_t), pdMS_TO_TICKS(1000));
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
                                if (resp_item && resp_item->valuestring) {
                                    wrap_text_to_lines(resp_item->valuestring, speaking_lines, &speaking_line_count);
                                    speaking_scroll_index = 0;
                                    speaking_scroll_timer = 0;
                                } else {
                                    speaking_line_count = 0;
                                }
                                update_state(APP_SPEAKING);
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
                            } else if (strcmp(event_item->valuestring, "error") == 0) {
                                ESP_LOGE(TAG, "Server pipeline error event received!");
                                update_state(APP_ERROR);
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
        .buffer_size = 4096,
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
    ESP_LOGI(TAG, "      ZYRA v%s", ZYRA_VERSION);
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
            oled_print_string(&display_device, 0, 1, ZYRA_NAME);
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

    // Create background audio recording task pinned to Core 1
    xTaskCreatePinnedToCore(audio_record_task, "audio_record", 4096, NULL, 5, NULL, 1);

    // Create background OLED display animation task
    xTaskCreate(display_animation_task, "display_anim", 4096, NULL, 2, NULL);

    // 6. Test Speaker Tone Playback (440Hz for 500ms)
    if (display_device.initialized) {
        oled_clear(&display_device);
        oled_print_string(&display_device, 0, 1, ZYRA_NAME);
        oled_print_string(&display_device, 0, 3, "Testing Speaker...");
        oled_refresh(&display_device);
    }
    play_tone_440hz(&spk_device, 500);

    // 7. Initialize WebSocket connection
    if (wifi_manager_is_connected()) {
        websocket_init();
    }

    ESP_LOGI(TAG, "ZYRA initialization complete. Ready for triggers.");

    while (true)
    {
        // Wait for system to be connected and idle
        while (g_state == APP_BOOT || g_state == APP_WIFI_CONNECTING || g_state == APP_ERROR) {
            if (g_state == APP_ERROR) {
                if (wifi_manager_is_connected() && !ws_connected) {
                    // Try to reconnect websocket - destroy old client first
                    ESP_LOGI(TAG, "Attempting to reconnect WebSocket...");
                    if (ws_client) {
                        esp_websocket_client_stop(ws_client);
                        esp_websocket_client_destroy(ws_client);
                        ws_client = NULL;
                    }
                    websocket_init();
                } else if (ws_connected) {
                    // We are still connected, meaning it was a server/pipeline error.
                    // Play error beep tones, wait a bit, then recover back to APP_IDLE.
                    ESP_LOGW(TAG, "Server pipeline error. Recovering to idle state...");
                    play_beep(&spk_device, 220.0, 150);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    play_beep(&spk_device, 220.0, 150);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    update_state(APP_IDLE);
                    break;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(5000));
        }

        // Check if button is pressed (active-low GPIO 0)
        if (button_is_pressed(&trg_button) && ws_connected && ws_client) {
            ESP_LOGI(TAG, "Button pressed! Sending manual start event...");
            const char* start_msg = "{\"event\": \"start\"}";
            esp_websocket_client_send_text(ws_client, start_msg, strlen(start_msg), pdMS_TO_TICKS(1000));
            update_state(APP_LISTENING);

            // Wait until button is released
            while (button_is_pressed(&trg_button)) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }

            ESP_LOGI(TAG, "Button released! Sending manual stop event...");
            const char* stop_msg = "{\"event\": \"stop\"}";
            esp_websocket_client_send_text(ws_client, stop_msg, strlen(stop_msg), pdMS_TO_TICKS(1000));
            update_state(APP_PROCESSING);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
