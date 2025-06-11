#pragma once

#include "basic_types.h"

inline constexpr s32 WAV_HEADER_SIZE = 44;

sizet write_wav_to_file(const char *path, const f32 *pcm_data, s32 sample_rate, s32 num_channels, s32 num_samples);

// Buffer must be large enough for pcm data + wav header
bool write_wav_to_buffer(u8 *wav_data, const f32 *pcm_data, s32 sample_rate, s32 num_channels, s32 num_samples);

// Allocates buffer and writes wav header and pcm data to it - fills buffer_size with the byte size of the allocated
// buffer. User is responsible to free on buffer when done.
u8* create_wav_buffer(sizet *buffer_size, const f32 *pcm_data, s32 sample_rate, s32 num_channels, s32 num_samples);
