#include "api.h"
#include <stdlib.h>
#include <string.h>

#pragma pack(push, 1)
struct WavHeader {
    char riff[4];
    uint32_t overall_size;
    char wave[4];
    char fmt_chunk_marker[4];
    uint32_t length_of_fmt;
    uint16_t format_type;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byterate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data_chunk_header[4];
    uint32_t data_size;
};
#pragma pack(pop)

void audio_buffer_init(audio_buffer_t* buf, size_t max_size_bytes) {
    if (!buf) return;
    buf->max_size = max_size_bytes;
    if (buf->max_size < 44) {
        buf->max_size = 44;
    }
    buf->buffer = (uint8_t*)malloc(buf->max_size);
    audio_buffer_clear(buf);
}

void audio_buffer_free(audio_buffer_t* buf) {
    if (buf && buf->buffer) {
        free(buf->buffer);
        buf->buffer = NULL;
    }
}

void audio_buffer_clear(audio_buffer_t* buf) {
    if (!buf) return;
    buf->pcm_size = 0;
    if (buf->buffer) {
        memset(buf->buffer, 0, 44);
    }
}

bool audio_buffer_write(audio_buffer_t* buf, const uint8_t* data, size_t size) {
    if (!buf || !buf->buffer || buf->pcm_size + size + 44 > buf->max_size) {
        return false;
    }
    memcpy(buf->buffer + 44 + buf->pcm_size, data, size);
    buf->pcm_size += size;
    return true;
}

uint8_t* audio_buffer_get_pcm_write_ptr(audio_buffer_t* buf) {
    if (!buf || !buf->buffer) return NULL;
    return buf->buffer + 44 + buf->pcm_size;
}

uint8_t* audio_buffer_get_wav_pointer(audio_buffer_t* buf) {
    if (!buf) return NULL;
    return buf->buffer;
}

size_t audio_buffer_get_wav_size(const audio_buffer_t* buf) {
    if (!buf) return 0;
    return buf->pcm_size + 44;
}

size_t audio_buffer_get_pcm_size(const audio_buffer_t* buf) {
    if (!buf) return 0;
    return buf->pcm_size;
}

size_t audio_buffer_get_free_space(const audio_buffer_t* buf) {
    if (!buf || buf->max_size < 44 + buf->pcm_size) return 0;
    return buf->max_size - 44 - buf->pcm_size;
}

void audio_buffer_update_wav_header(audio_buffer_t* buf, uint32_t sample_rate) {
    if (!buf || !buf->buffer) return;

    struct WavHeader header = {
        .riff = {'R', 'I', 'F', 'F'},
        .overall_size = (uint32_t)(buf->pcm_size + 36),
        .wave = {'W', 'A', 'V', 'E'},
        .fmt_chunk_marker = {'f', 'm', 't', ' '},
        .length_of_fmt = 16,
        .format_type = 1,
        .channels = 1,
        .sample_rate = sample_rate,
        .byterate = sample_rate * 1 * 16 / 8,
        .block_align = 1 * 16 / 8,
        .bits_per_sample = 16,
        .data_chunk_header = {'d', 'a', 't', 'a'},
        .data_size = (uint32_t)buf->pcm_size
    };

    memcpy(buf->buffer, &header, 44);
}
