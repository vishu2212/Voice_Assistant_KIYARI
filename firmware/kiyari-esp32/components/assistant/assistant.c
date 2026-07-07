#include "assistant.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char* TAG = "VoiceAssistantClient";

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
        while (*str && *str != ' ' && *str != '\n' && word_len < sizeof(word) - 1) {
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

void assistant_init(assistant_t* client, oled_display_t* display, speaker_t* speaker, const char* base_url) {
    if (!client) return;
    client->display = display;
    client->speaker = speaker;
    strncpy(client->base_url, base_url, sizeof(client->base_url) - 1);
    client->base_url[sizeof(client->base_url) - 1] = '\0';
}

esp_err_t assistant_send_voice_chat(assistant_t* client, audio_buffer_t* audio_buf) {
    if (!client || !client->display || !audio_buf) return ESP_ERR_INVALID_ARG;

    char url[256];
    snprintf(url, sizeof(url), "%s/transcribe", client->base_url);
    ESP_LOGI(TAG, "Uploading WAV payload to: %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 15000,
    };

    esp_http_client_handle_t http_client = esp_http_client_init(&config);
    if (!http_client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    // Set headers for multipart form data
    const char* boundary = "----ESP32Boundary12345";
    char content_type[128];
    snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", boundary);
    esp_http_client_set_header(http_client, "Content-Type", content_type);

    // Build the multipart body parts (Fix: changed form-data name field from "file" to "audio")
    char body_start[256];
    int body_start_len = snprintf(body_start, sizeof(body_start),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"audio\"; filename=\"audio.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n", boundary);

    char body_end[64];
    int body_end_len = snprintf(body_end, sizeof(body_end), "\r\n--%s--\r\n", boundary);

    size_t wav_data_size = audio_buffer_get_wav_size(audio_buf);
    uint8_t* wav_data = audio_buffer_get_wav_pointer(audio_buf);

    size_t total_payload_size = body_start_len + wav_data_size + body_end_len;
    
    esp_err_t err = esp_http_client_open(http_client, total_payload_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(http_client);
        return err;
    }

    // Write body start
    esp_http_client_write(http_client, body_start, body_start_len);
    // Write WAV audio data
    esp_http_client_write(http_client, (const char*)wav_data, wav_data_size);
    // Write body end
    esp_http_client_write(http_client, body_end, body_end_len);

    int content_length = esp_http_client_fetch_headers(http_client);
    int status_code = esp_http_client_get_status_code(http_client);
    ESP_LOGI(TAG, "HTTP POST status code: %d, content length: %d", status_code, content_length);

    if (status_code == 200 && content_length > 0) {
        char* response_buffer = (char*)malloc(content_length + 1);
        if (!response_buffer) {
            ESP_LOGE(TAG, "Failed to allocate response buffer");
            esp_http_client_cleanup(http_client);
            return ESP_ERR_NO_MEM;
        }

        int read_len = esp_http_client_read(http_client, response_buffer, content_length);
        if (read_len >= 0) {
            response_buffer[read_len] = '\0';
            ESP_LOGI(TAG, "Response: %s", response_buffer);

            cJSON* root = cJSON_Parse(response_buffer);
            if (root) {
                cJSON* text_item = cJSON_GetObjectItem(root, "text");

                oled_clear(client->display);
                if (text_item && text_item->valuestring) {
                    oled_print_wrapped_string(client->display, 0, "You said:", text_item->valuestring);
                } else {
                    oled_print_string(client->display, 0, 3, "No response text");
                }
                oled_refresh(client->display);
                cJSON_Delete(root);
            } else {
                ESP_LOGE(TAG, "JSON parse failure");
            }
        }
        free(response_buffer);
    } else {
        ESP_LOGE(TAG, "HTTP POST Request failed with status: %d", status_code);
    }

    esp_http_client_cleanup(http_client);
    return (status_code == 200) ? ESP_OK : ESP_FAIL;
}
