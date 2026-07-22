#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint8_t* buffer;
    size_t max_size;
    size_t pcm_size;
} audio_buffer_t;

// Allocates heap memory for the audio buffer structure
void audio_buffer_init(audio_buffer_t* buf, size_t max_size_bytes);

// Frees the allocated heap memory
void audio_buffer_free(audio_buffer_t* buf);

// Resets buffer indices
void audio_buffer_clear(audio_buffer_t* buf);

// Writes sample bytes into buffer after WAV header
bool audio_buffer_write(audio_buffer_t* buf, const uint8_t* data, size_t size);

// Returns pointer to memory offset where raw PCM data is recorded
uint8_t* audio_buffer_get_pcm_write_ptr(audio_buffer_t* buf);

// Returns pointer to the start of the full WAV file stream
uint8_t* audio_buffer_get_wav_pointer(audio_buffer_t* buf);

// Gets the current WAV file total length
size_t audio_buffer_get_wav_size(const audio_buffer_t* buf);

// Gets the current raw PCM length
size_t audio_buffer_get_pcm_size(const audio_buffer_t* buf);

// Gets remaining recording capacity size in bytes
size_t audio_buffer_get_free_space(const audio_buffer_t* buf);

// Writes standard 44-byte WAV header parameters
void audio_buffer_update_wav_header(audio_buffer_t* buf, uint32_t sample_rate);
