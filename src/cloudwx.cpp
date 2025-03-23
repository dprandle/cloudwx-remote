#include <malloc.h>
#include <cstring>
#include <cerrno>
#include <atomic>
#include <sys/stat.h>

#include "cloudwx.h"
#include "cli_example.h"
#include "miniaudio.h"
#include "whisper.h"
#include "logging.h"
#include "basic_types.h"
#include "utils.h"

using namespace nslib;

// Amount of bytes we want to process at a time in whisper - 1024 frames come in at a time with 1 sample per frame and
// the sample rate is 16kHz, so we get 16k samples per second. We want to process 10 seconds worth of audio at a time.
intern constexpr sizet AUDIO_CHUNK_SAMPLE_COUNT = 1024 * 16 * 30;
intern constexpr sizet AUDIO_BUFFER_SAMPLE_COUNT = AUDIO_CHUNK_SAMPLE_COUNT * 10;

struct ring_buffer
{
    // Shared buffer in both threads - big enough it shouldnt ever matter
    f32 *buffer;
    // Data available for processing - incremented by sound thread and decremented by processing thread
    std::atomic_size_t available;
    // Used by sound thread only
    size_t write_pos;
    // Used by processing thread only
    size_t read_pos;
};

struct miniaudio_ctxt
{
    ma_log lg;
    ma_context ctxt;
    ma_device dev;
    ring_buffer data;
};

intern FILE *open_logging_file()
{
    // Format of timestamp
    time_t t = time(NULL);
    auto ct = localtime(&t);

    char path[48];
    path[strftime(path, sizeof(path), "%Y/%m(%b)/%d(%a)", ct)] = '\0';
    int result = mkdir_p(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (result == -1 && errno != EEXIST) {
        elog("Could not create directory %s for logging: %s", path, strerror(errno));
        return nullptr;
    }

    char fname[16];
    fname[strftime(fname, sizeof(fname), "/%H:%M:%S.log", ct)] = '\0';
    strcat(path, fname);

    auto f = fopen(path, "w");
    if (!f) {
        elog("Could not open file %s for logging: %s", path, strerror(errno));
    }
    ilog("Opened %s for logging", path);
    return f;
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

intern void terminate_audio(miniaudio_ctxt *ma)
{
    ilog("Terminating audio");
    ma_device_uninit(&ma->dev);
    free(ma->data.buffer);
    ma_context_uninit(&ma->ctxt);
    ma_log_uninit(&ma->lg);
}

intern void audio_callback(ma_device *dev, void *output, const void *input, u32 frame_count)
{
    auto ma = (miniaudio_ctxt *)dev->pUserData;
    // Frames are same as sample count since we have mono audio
    u32 sample_count = frame_count * 1;
    static int silent_frames = 0;
    sizet byte_cnt = sample_count * sizeof(f32);
    auto buffer_start = ma->data.buffer + ma->data.write_pos;
    u32 silence_count = 0;
    const float *input_f = (const float*)input;
    for (int i = 0; i < sample_count; ++i) {
        if (std::abs(input_f[i]) < 0.005f) {
            ++silence_count;
        }
        buffer_start[i] = input_f[i];
    }
    if (silence_count == sample_count) {
        ++silent_frames;
        ilog("%d silent frames", silent_frames);
    }
    else {
        silent_frames = 0;
    }
    //memcpy(buffer_start, input, byte_cnt);
    //ilog("Silent samples %d", silence_count);
    ma->data.write_pos += sample_count;

    if ((ma->data.write_pos % AUDIO_CHUNK_SAMPLE_COUNT) == 0) {
        std::atomic_fetch_add(&ma->data.available, AUDIO_CHUNK_SAMPLE_COUNT); // update available frames
    }

    assert(ma->data.write_pos <= AUDIO_BUFFER_SAMPLE_COUNT);
    if (ma->data.write_pos == AUDIO_BUFFER_SAMPLE_COUNT) {
        ma->data.write_pos = 0; // wrap around
    }
}

void process_available_audio(miniaudio_ctxt *ma, whisper_ctxt *whisper)
{
    if (std::atomic_load(&ma->data.available) < AUDIO_CHUNK_SAMPLE_COUNT) {
        return; // not enough data available
    }

    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);
    wparams.n_threads = 10;
    auto buffer_start = ma->data.buffer + ma->data.read_pos;
    tlog("Processing %u samples starting at offset %u", AUDIO_CHUNK_SAMPLE_COUNT, ma->data.read_pos);
    whisper_full(whisper, wparams, buffer_start, AUDIO_CHUNK_SAMPLE_COUNT);

    std::string tot_txt;
    const int n_segments = whisper_full_n_segments(whisper);
    for (int i = 0; i < n_segments; ++i) {
        const char *text = whisper_full_get_segment_text(whisper, i);
        tot_txt += text;
    }
    ilog("Chunk Text\n%s", tot_txt.c_str());

    ma->data.read_pos += AUDIO_CHUNK_SAMPLE_COUNT;
    assert(ma->data.read_pos <= AUDIO_BUFFER_SAMPLE_COUNT);
    if (ma->data.read_pos == AUDIO_BUFFER_SAMPLE_COUNT) {
        ma->data.read_pos = 0;
    }
    std::atomic_fetch_sub(&ma->data.available, AUDIO_CHUNK_SAMPLE_COUNT); // update available frames
}

intern bool init_audio(miniaudio_ctxt *ma)
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
        terminate_audio(ma);
        return false;
    }

    // Print out each device info and save the index of the USB device associated with what we want
    i32 our_dev_ind{-1};
    for (i32 devi = 0; devi < dev_cnt; devi += 1) {
        if (ma->ctxt.backend == ma_backend_alsa) {
            ilog("%d: %s : %s", devi, dev_infos[devi].name, dev_infos[devi].id.alsa);
            if (strstr(dev_infos[devi].name, "USB")) {
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
        config.capture.format = ma_format_f32;
        config.capture.channels = 1;
        config.sampleRate = 16000;
        config.dataCallback = audio_callback;
        config.pUserData = ma;

        ma_result result = ma_device_init(&ma->ctxt, &config, &ma->dev);
        if (result != MA_SUCCESS) {
            wlog("Could not initialize audio device %s: %s", dev_infos[our_dev_ind].name, ma_result_description(result));
            terminate_audio(ma);
            return false;
        }
    }
    else {
        wlog("Could not find USB audio device");
        terminate_audio(ma);
        return false;
    }

    ma->data.buffer = (f32 *)malloc(AUDIO_BUFFER_SAMPLE_COUNT * sizeof(f32));
    return true;
}

void whisper_log_callback(enum ggml_log_level level, const char *text, void *user_data)
{
    if (level >= GGML_LOG_LEVEL_DEBUG && level <= GGML_LOG_LEVEL_CONT) {
        log_at_level((int)level, false, text);
    }
}

intern bool init_whisper(cloudwx_ctxt *ctxt)
{
    ilog("Initializing whisper");
    whisper_log_set(whisper_log_callback, nullptr);

    whisper_context_params cparams = whisper_context_default_params();
    ctxt->whisper = whisper_init_from_file_with_params("models/ggml-tiny.en.bin", cparams);
    if (!ctxt->whisper) {
        wlog("Failed to initialize whisper context");
        return false;
    }
    return true;
}

intern void terminate_whisper(cloudwx_ctxt *ctxt)
{
    ilog("Terminating whisper");
    whisper_free(ctxt->whisper);
    ctxt->whisper = nullptr;
}

bool init_cloudwx(cloudwx_ctxt *ctxt)
{
    ilog("Initializing cloudwx");
    auto fp = open_logging_file();
    if (fp) {
        add_logging_fp(GLOBAL_LOGGER, fp, LOG_TRACE);
    }
#if defined(NDEBUG)
    set_logging_level(GLOBAL_LOGGER, LOG_INFO);
#else
    set_logging_level(GLOBAL_LOGGER, LOG_DEBUG);
#endif
    ctxt->ma = new miniaudio_ctxt{};
    if (!init_audio(ctxt->ma)) {
        delete ctxt->ma;
        return false;
    }
    if (!init_whisper(ctxt)) {
        terminate_audio(ctxt->ma);
        delete ctxt->ma;
        return false;
    }
    ma_device_start(&ctxt->ma->dev);
    return true;
}

void terminate_cloudwx(cloudwx_ctxt *ctxt)
{
    ilog("Terminating cloudwx");
    ma_device_stop(&ctxt->ma->dev);
    terminate_whisper(ctxt);
    terminate_audio(ctxt->ma);
    ctxt->ma = nullptr;
}
