#include <cstring>
#include <atomic>
#include <cmath>

#include "logging.h"
#include "miniaudio.h"
#include "work_queue.h"
#include "global_constants.h"
#include "audio.h"

intern constexpr sizet AUDIO_ENTRY_MAX_CHUNK_COUNT = (1000 / AUDIO_CHUNK_DURATION_MS) * AUDIO_ENTRY_MAX_DURATION_S;
intern constexpr u32 AUDIO_CB_CHUNK_FRAME_COUNT = (AUDIO_SAMPLE_RATE * AUDIO_CHUNK_DURATION_MS) / 1000;
intern constexpr u32 AUDIO_CB_CHUNK_SAMPLE_COUNT = AUDIO_CB_CHUNK_FRAME_COUNT * AUDIO_CHANNEL_COUNT;
// Triple buffer the audio
intern constexpr sizet AUDIO_BUFFER_SAMPLE_COUNT = AUDIO_SAMPLE_RATE * AUDIO_ENTRY_MAX_DURATION_S * 3;
intern constexpr sizet CONSECUTIVE_SILENT_AUDIO_CHUNK_THRESHOLD = (CONSECUTIVE_SILENT_AUDIO_THRESHOLD_MS / AUDIO_CHUNK_DURATION_MS);

inline constexpr s32 WAV_HEADER_SIZE = 44;
intern constexpr s16 MAX_S16 = std::numeric_limits<s16>::max();
intern constexpr f32 SAMPLE_RMS_DENOM = MAX_S16 * MAX_S16;

struct proc_thread_audio_data
{
    size_t read_pos;
};

struct snd_thread_audio_data
{
    size_t write_pos;
    size_t consecutive_silent_chunks{};
    bool recording;
    s32 buffered_chunk_cnt{};
};

struct audio_buffer
{
    // Shared buffer in both threads - big enough it shouldnt ever matter
    s16 *buffer;
    // Data available for processing - incremented by sound thread and decremented by processing thread
    std::atomic_size_t available;
    // Used by processing thread only
    proc_thread_audio_data proc_data;
    // Used by the sound thread only
    snd_thread_audio_data snd_data;
};

struct audio_ctxt
{
    ma_log lg;
    ma_context ctxt;
    ma_device dev;
    audio_buffer data;
};

struct upload_audio_chunk_data
{
    s16 *pcm_data;
    sizet sample_count;
};

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
void write_wav_header(FILE *f, s32 sample_rate, s32 num_channels, s32 num_samples)
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

intern void on_ma_log(void *, u32 level, const char *msg)
{
    switch (level) {
    case (MA_LOG_LEVEL_DEBUG):
        log_at_level(LOG_DEBUG, false, msg);
        break;
    case (MA_LOG_LEVEL_INFO):
        log_at_level(LOG_INFO, false, msg);
        break;
    case (MA_LOG_LEVEL_WARNING):
        log_at_level(LOG_WARN, false, msg);
        break;
    case (MA_LOG_LEVEL_ERROR):
        log_at_level(LOG_ERROR, false, msg);
        break;
    default:
        log_at_level(LOG_TRACE, false, msg);
    }
}

intern void stop_recording(snd_thread_audio_data *snd_data)
{
    dlog("Recording stop at write pos %u with %u available chunks", snd_data->write_pos, snd_data->buffered_chunk_cnt);
    snd_data->recording = false;
}

intern void audio_callback(ma_device *dev, void *output, const void *input, u32 frame_count)
{
    auto ma = (audio_ctxt *)dev->pUserData;

    // Frames are same as sample count since we have mono audio
    u32 sample_count = frame_count * AUDIO_CHANNEL_COUNT;
    const s16 *input_f = (const s16 *)input;

    // Calculate our chunk RMS
    float sample_rms{};
    for (int i = 0; i < sample_count; ++i) {
        sample_rms += (input_f[i] * input_f[i] / SAMPLE_RMS_DENOM);
    }
    sample_rms = sqrt(sample_rms / sample_count);

    bool update_avail{false};
    if (sample_rms < AUDIO_SILENT_THRESHOLD_RMS) {
        ++ma->data.snd_data.consecutive_silent_chunks;
        // dlog("Increased silence count to %d", ma->data.snd_data.consecutive_silent_chunks);
        assert(ma->data.snd_data.consecutive_silent_chunks <= CONSECUTIVE_SILENT_AUDIO_CHUNK_THRESHOLD);
        if (ma->data.snd_data.consecutive_silent_chunks == CONSECUTIVE_SILENT_AUDIO_CHUNK_THRESHOLD) {
            ma->data.snd_data.consecutive_silent_chunks = 0;
            if (ma->data.snd_data.recording) {
                stop_recording(&ma->data.snd_data);
                dlog("Stopped due to silence");
                update_avail = true;
            }
        }
    }
    else {
        ma->data.snd_data.consecutive_silent_chunks = 0;
        if (!ma->data.snd_data.recording) {
            dlog("Recording start at write pos %u with %u available chunks", ma->data.snd_data.buffered_chunk_cnt);
            ma->data.snd_data.recording = true;
        }
    }

    if (ma->data.snd_data.recording) {
        auto write_buffer = ma->data.buffer + ma->data.snd_data.write_pos;
        size_t in_buf_cpy_offset{};
        size_t new_pos = ma->data.snd_data.write_pos + sample_count;
        if (new_pos >= AUDIO_BUFFER_SAMPLE_COUNT) {
            auto in_buf_cpy_offset = (AUDIO_BUFFER_SAMPLE_COUNT - ma->data.snd_data.write_pos);
            memcpy(write_buffer, input, in_buf_cpy_offset * sizeof(s16));
            write_buffer = ma->data.buffer;
            new_pos -= AUDIO_BUFFER_SAMPLE_COUNT;
        }
        else if (new_pos == AUDIO_BUFFER_SAMPLE_COUNT) {
            new_pos = 0;
        }
        memcpy(write_buffer, input_f + in_buf_cpy_offset, (sample_count - in_buf_cpy_offset) * sizeof(s16));
        ++ma->data.snd_data.buffered_chunk_cnt;
        ma->data.snd_data.write_pos = new_pos;
        // dlog("Recorded chunk resulting in write pos %u", ma->data.snd_data.buffered_chunk_cnt);
        assert(ma->data.snd_data.buffered_chunk_cnt <= AUDIO_ENTRY_MAX_CHUNK_COUNT);
        if (ma->data.snd_data.buffered_chunk_cnt == AUDIO_ENTRY_MAX_CHUNK_COUNT) {
            stop_recording(&ma->data.snd_data);
            dlog("Stopped due to threshold");
            update_avail = true;
        }
    }

    if (update_avail) {
        auto res = std::atomic_fetch_add(&ma->data.available, ma->data.snd_data.buffered_chunk_cnt);
        std::atomic_notify_one(&ma->data.available);
        dlog("Updating available chunk count from %lu to %lu", res, res + ma->data.snd_data.buffered_chunk_cnt);
        ma->data.snd_data.buffered_chunk_cnt = 0;
    }
}

intern void upload_audio_chunk_with_meta(void *arg)
{
    auto chunk_data = (upload_audio_chunk_data *)arg;
    char fname[16]{};
    static int chunks = 0;
    sprintf(fname, "chunk_%d.wav", chunks++);
    write_wav_to_file(fname, chunk_data->pcm_data, AUDIO_SAMPLE_RATE, AUDIO_CHANNEL_COUNT, chunk_data->sample_count);
    ilog("Saving %lu sample audio chunk to %s", chunk_data->sample_count, fname);
    free(chunk_data->pcm_data);
    free(chunk_data);
}

void process_available_audio(audio_ctxt *ma, work_queue *wq)
{
    std::atomic_wait(&ma->data.available, 0);
    size_t avail_chunks = std::atomic_load(&ma->data.available);
    asrt(avail_chunks > 0);

    auto chunk_data = (upload_audio_chunk_data *)calloc(1, sizeof(upload_audio_chunk_data));
    chunk_data->sample_count = avail_chunks * AUDIO_CB_CHUNK_SAMPLE_COUNT;
    chunk_data->pcm_data = (s16 *)calloc(1, chunk_data->sample_count * sizeof(s16));

    // Get the end position of the read buffer - if its past the end of the audio circular buffer (>
    // AUDIO_BUFFER_SAMPLE_COUNT) then we copy the chunk at the end of the buffer first, then copy the chunk from the
    // start of the buffer to the recalculated end pos..
    size_t read_buf_end_pos = ma->data.proc_data.read_pos + chunk_data->sample_count;
    auto dest_buf = chunk_data->pcm_data;
    if (read_buf_end_pos > AUDIO_BUFFER_SAMPLE_COUNT) {
        // Copy the end chunk of the audio buffer
        auto read_buf_partial_sz = (AUDIO_BUFFER_SAMPLE_COUNT - ma->data.proc_data.read_pos);
        ilog("Copying %u samples from audio (%u offset) to pcm buffer", read_buf_partial_sz, ma->data.proc_data.read_pos);
        memcpy(chunk_data->pcm_data, ma->data.buffer + ma->data.proc_data.read_pos, read_buf_partial_sz * sizeof(s16));

        // Now set the read pos to the start of the cirular buffer, and recalculate the end pos. We also adjust the dest
        // buffer to account for the samples we just wrote to it
        ma->data.proc_data.read_pos = 0;
        read_buf_end_pos -= AUDIO_BUFFER_SAMPLE_COUNT;
        dest_buf += read_buf_partial_sz;
    }
    ilog("Copying %u samples from audio (%u read_pos) to pcm buffer (offset %u)",
         (read_buf_end_pos - ma->data.proc_data.read_pos),
         ma->data.proc_data.read_pos,
         dest_buf - chunk_data->pcm_data);
    memcpy(dest_buf, ma->data.buffer + ma->data.proc_data.read_pos, (read_buf_end_pos - ma->data.proc_data.read_pos) * sizeof(s16));
    // If read_buf_end_pos is AUDIO_BUFFER_SAMPLE_COUNT, it will be correctly handled on the next set of samples.. the first
    // partial memcpy would just copy 0 bytes as read_buf_partial_sz would calculate to 0
    ma->data.proc_data.read_pos = read_buf_end_pos;

    auto res = std::atomic_fetch_sub(&ma->data.available, avail_chunks); // update available frames
    ilog("Updating available chunk count from %lu to %lu", res, res - avail_chunks);

    enqueue_task(wq, {upload_audio_chunk_with_meta, chunk_data});
}

bool audio_init(audio_ctxt *ma)
{
    ilog("Initializing audio");
    ma_result result = ma_log_init(nullptr, &ma->lg);
    if (result != MA_SUCCESS) {
        wlog("Failed to initialize log: %s", ma_result_description(result));
        return false;
    }
    ma_log_callback cb{};
    cb.onLog = on_ma_log;
    ma_log_register_callback(&ma->lg, cb);

    ma_context_config ccfg{};
    ccfg.pLog = &ma->lg;

    result = ma_context_init(NULL, 0, &ccfg, &ma->ctxt);
    if (result != MA_SUCCESS) {
        wlog("Could not initialize audio context - err: %s", ma_result_description(result));
        ma_log_uninit(&ma->lg);
        return false;
    }
    ilog("Selected audio backend: %s", ma_get_backend_name(ma->ctxt.backend));

    ma_device_info *dev_infos;
    ma_uint32 dev_cnt;
    ma_device_info *capture_infos;
    u32 capture_dev_cnt;
    result = ma_context_get_devices(&ma->ctxt, &dev_infos, &dev_cnt, &capture_infos, &capture_dev_cnt);
    if (result != MA_SUCCESS) {
        wlog("Could not list audio devices: %s", ma_result_description(result));
        audio_terminate(ma);
        return false;
    }

    // Print out each device info and save the index of the USB device associated with what we want
    s32 our_dev_ind{-1};
    for (s32 devi = 0; devi < dev_cnt; devi += 1) {
        if (ma->ctxt.backend == ma_backend_alsa) {
            ilog("%d: %s : %s", devi, dev_infos[devi].name, dev_infos[devi].id.alsa);
            if (strstr(dev_infos[devi].name, "USB Audio CODEC")) {
                our_dev_ind = devi;
            }
        }
        else if (ma->ctxt.backend == ma_backend_pulseaudio) {
            ilog("%d: %s : %s", devi, dev_infos[devi].name, dev_infos[devi].id.pulse);
        }
        else if (ma->ctxt.backend == ma_backend_jack) {
            ilog("%d: %s : %d", devi, dev_infos[devi].name, dev_infos[devi].id.jack);
        }
        else {
            ilog("%d - %s", devi, dev_infos[devi].name);
        }
    }
    if (our_dev_ind != -1) {
        ma_device_config config = ma_device_config_init(ma_device_type_capture);
        config.capture.pDeviceID = &dev_infos[our_dev_ind].id;
        config.capture.format = ma_format_s16;
        config.capture.channels = AUDIO_CHANNEL_COUNT;
        config.sampleRate = AUDIO_SAMPLE_RATE;
        config.periodSizeInFrames = AUDIO_CB_CHUNK_FRAME_COUNT;
        config.dataCallback = audio_callback;
        config.pUserData = ma;

        ma_result result = ma_device_init(&ma->ctxt, &config, &ma->dev);
        if (result != MA_SUCCESS) {
            wlog("Could not initialize audio device %s: %s", dev_infos[our_dev_ind].name, ma_result_description(result));
            audio_terminate(ma);
            return false;
        }
    }
    else {
        wlog("Could not find USB audio device");
        audio_terminate(ma);
        return false;
    }
    sizet buf_sz = AUDIO_BUFFER_SAMPLE_COUNT * sizeof(s16);
    ilog("Allocating %u byte buffer for audio recording", buf_sz);
    ma->data.buffer = (s16 *)malloc(buf_sz);

    ma_device_start(&ma->dev);
    return true;
}

void audio_terminate(audio_ctxt *aud)
{
    ilog("Terminating audio");
    if (ma_device_is_started(&aud->dev)) {
        ma_device_stop(&aud->dev);
    }
    ma_device_uninit(&aud->dev);
    free(aud->data.buffer);
    ma_context_uninit(&aud->ctxt);
    ma_log_uninit(&aud->lg);
}

audio_ctxt *audio_create()
{
    return new audio_ctxt{};
}

void audio_destroy(audio_ctxt *aud)
{
    delete aud;
}
