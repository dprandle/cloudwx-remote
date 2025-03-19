#include <cstring>
#include <cerrno>
#include <sys/stat.h>

#include "cloudwx.h"
#include "miniaudio.h"
#include "whisper.h"
#include "logging.h"
#include "basic_types.h"
#include "utils.h"

using namespace nslib;

struct miniaudio_ctxt
{
    ma_log lg;
    ma_context ctxt;
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
    ma_context_uninit(&ma->ctxt);
    ma_log_uninit(&ma->lg);
    *ma = {};
}

intern bool init_audio(miniaudio_ctxt *ma)
{
    ilog("Initializing audio");
    int result = ma_log_init(nullptr, &ma->lg);
    ma_log_callback cb{};
    cb.onLog = on_ma_log;
    ma_log_register_callback(&ma->lg, cb);

    ma_context_config ccfg{};
    ccfg.pLog = &ma->lg;
    ccfg.threadPriority = ma_thread_priority_realtime;

    result = ma_context_init(NULL, 0, &ccfg, &ma->ctxt);
    if (result != MA_SUCCESS) {
        wlog("Could not initialize audio context - err code: %d", result);
        ma_log_uninit(&ma->lg);
        return false;
    }

    ma_device_info *dev_infos;
    ma_uint32 dev_cnt;
    ma_device_info *capture_infos;
    u32 capture_dev_cnt;
    result = ma_context_get_devices(&ma->ctxt, &dev_infos, &dev_cnt, &capture_infos, &capture_dev_cnt);
    if (result != MA_SUCCESS) {
        wlog("Could not list audio devices - err code: %d", result);
        terminate_audio(ma);
        return false;
    }

    // Loop over each device info and do something with it. Here we just print the name with their index. You may want
    // to give the user the opportunity to choose which device they'd prefer.
    for (i32 devi = 0; devi < dev_cnt; devi += 1) {
        ilog("%d - %s", devi, dev_infos[devi].name);
    }

    // ma_device_config config = ma_device_config_init(ma_device_type_playback);
    // config.playback.pDeviceID = &dev_infos[chosenPlaybackDeviceIndex].id;
    // config.playback.format    = MY_FORMAT;
    // config.playback.channels  = MY_CHANNEL_COUNT;
    // config.sampleRate         = MY_SAMPLE_RATE;
    // config.dataCallback       = data_callback;
    // config.pUserData          = pMyCustomData;

    // ma_device device;
    // if (ma_device_init(&context, &config, &device) != MA_SUCCESS) {
    //     // Error
    // }
    return true;
}

intern bool init_whisper(cloudwx_ctxt *ctxt)
{
    ilog("Initializing whisper");
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
    return true;
}

void terminate_cloudwx(cloudwx_ctxt *ctxt)
{
    ilog("Terminating cloudwx");
    terminate_whisper(ctxt);
    terminate_audio(ctxt->ma);
    ctxt->ma = nullptr;
}
