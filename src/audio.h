#pragma once

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include "basic_types.h"
#include "logging.h"

inline constexpr s32 WAV_HEADER_SIZE = 44;

inline sizet wav_pcm_data_size(s32 num_channels, s32 num_samples)
{
    return num_channels * num_samples;
}

template<class T>
sizet wav_file_sizeof(s32 num_channels, s32 num_samples)
{
    return WAV_HEADER_SIZE + wav_pcm_data_size(num_channels, num_samples) * sizeof(T);
}

template<class T>
intern void write_wav_header(FILE *f, s32 sample_rate, s32 num_channels, s32 num_samples)
{
    s32 byte_rate = sample_rate * num_channels * sizeof(T);
    s32 data_chunk_size = num_samples * num_channels * sizeof(T);
    s32 fmt_chunk_size = 16;
    s32 wav_header_size = 44;

    fwrite("RIFF", 1, 4, f);
    u32 chunk_size = data_chunk_size + wav_header_size - 8;
    fwrite(&chunk_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);

    // fmt chunk
    fwrite("fmt ", 1, 4, f);
    u32 fmt_size = fmt_chunk_size;
    fwrite(&fmt_size, 4, 1, f);
    u16 audio_format = std::is_floating_point_v<T> ? 3 : 1; // 3 for float, 1 for PCM
    fwrite(&audio_format, 2, 1, f);
    fwrite(&num_channels, 2, 1, f);
    fwrite(&sample_rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    u16 block_align = num_channels * sizeof(T);
    fwrite(&block_align, 2, 1, f);
    u16 bits_per_sample = sizeof(T) * 8;
    fwrite(&bits_per_sample, 2, 1, f);

    // data chunk
    fwrite("data", 1, 4, f);
    fwrite(&data_chunk_size, 4, 1, f);
}

template<class T>
sizet write_wav_to_file(const char *path, const T *pcm_data, s32 sample_rate, s32 num_channels, s32 num_samples)
{
    asrt(path);
    FILE *f = fopen(path, "wb");
    if (!f) {
        elog("Could not open file %s for writing: %s", path, strerror(errno));
        return 0;
    }
    write_wav_header<T>(f, sample_rate, num_channels, num_samples);
    // Write audio data here (for example purposes, we write silence)
    auto ret = fwrite(pcm_data, sizeof(T), num_samples * num_channels, f);
    fclose(f);
    return ret;
}

template<class T>
sizet write_wav_to_buffer(u8 *wav_data_buffer, sizet wav_data_buffer_size, const T *pcm_data, s32 sample_rate, s32 num_channels, s32 num_samples)
{
    asrt(wav_data_buffer);
    asrt(pcm_data);
    auto bytes = wav_file_sizeof<T>(num_channels, num_samples);
    asrt(wav_data_buffer_size >= bytes);

    auto f = fmemopen(wav_data_buffer, bytes, "wb");
    if (!f) {
        elog("Could not open membuf of %lu bytes: %s", bytes, strerror(errno));
        return 0;
    }

    write_wav_header<T>(f, sample_rate, num_samples, num_samples);

    // Write audio data here (for example purposes, we write silence)
    auto ret = fwrite(pcm_data, sizeof(T), wav_pcm_data_size(num_channels, num_samples), f);
    fclose(f);
    return ret;
}

template<class T>
u8 *create_wav_buffer(sizet *buffer_size, const T *pcm_data, s32 sample_rate, s32 num_channels, s32 num_samples)
{
    *buffer_size = wav_file_sizeof<T>(num_channels, num_samples);
    u8 *buffer = (u8 *)calloc(1, *buffer_size);
    if (!buffer) {
        elog("Failed to allocate %lu bytes for WAV buffer", *buffer_size);
        return nullptr;
    }
    auto ret = write_wav_to_buffer(buffer, *buffer_size, pcm_data, sample_rate, num_channels, num_samples);
    asrt(ret == *buffer_size);
    return buffer;
}
