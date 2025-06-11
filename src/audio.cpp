#include <cerrno>
#include <cstdio>
#include <cstring>
#include "audio.h"
#include "logging.h"

intern void write_wav_header(FILE *f, s32 sample_rate, s32 num_channels, s32 num_samples)
{
    s32 byte_rate = sample_rate * num_channels * 2; // 16-bit = 2 bytes
    s32 data_chunk_size = num_samples * num_channels * 2;
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
    u16 audio_format = 1; // PCM
    fwrite(&audio_format, 2, 1, f);
    fwrite(&num_channels, 2, 1, f);
    fwrite(&sample_rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    u16 block_align = num_channels * 2;
    fwrite(&block_align, 2, 1, f);
    u16 bits_per_sample = 16;
    fwrite(&bits_per_sample, 2, 1, f);

    // data chunk
    fwrite("data", 1, 4, f);
    fwrite(&data_chunk_size, 4, 1, f);
}

sizet write_wav_to_file(const char *path, const f32 *pcm_data, s32 sample_rate, s32 num_channels, s32 num_samples)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        elog("Could not open file %s for writing: %s", path, strerror(errno));
        return 0;
    }
    write_wav_header(f, sample_rate, num_channels, num_samples);
    // Write audio data here (for example purposes, we write silence)
    auto ret = fwrite(pcm_data, sizeof(f32), num_samples * num_channels, f);
    fclose(f);
    return ret;
}

bool write_wav_to_buffer(u8 *wav_data, const f32 *pcm_data, s32 sample_rate, s32 num_channels, s32 num_samples)
{
    asrt(wav_data);
    if (!f) {
        elog("Could not open file %s for writing: %s", path, strerror(errno));
        return false;
    }
    write_wav_header(f, sample_rate, num_channels, num_samples);
    // Write audio data here (for example purposes, we write silence)
    fwrite(data, sizeof(f32), num_samples * num_channels, f);
    fclose(f);
    return true;
}
