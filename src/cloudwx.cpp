#include <malloc.h>
#include <cstring>
#include <cerrno>
#include <cmath>

#include <sys/stat.h>

#include "cloudwx.h"
#include "audio.h"
#include "mongodb.h"
#include "miniaudio.h"
#include "logging.h"
#include "utils.h"


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

intern bool init_mongodb(cloudwx_ctxt *ctxt)
{
    mongodb_global_init();
    ctxt->db = mongodb_create(URI_PROD_SERVER, DB_NAME);
    if (!mongodb_init(ctxt->db)) {
        mongodb_destroy(ctxt->db);
        ctxt->db = nullptr;
    }
    return ctxt->db != nullptr;
}

intern void terminate_mongodb(cloudwx_ctxt *ctxt)
{
    mongodb_terminate(ctxt->db);
    mongodb_destroy(ctxt->db);
    mongodb_global_terminate();
}

intern bool init_audio(cloudwx_ctxt *ctxt)
{
    ctxt->ma = audio_create();
    if (!audio_init(ctxt->ma)) {
        audio_destroy(ctxt->ma);
        ctxt->ma = nullptr;
    }
    return ctxt->ma != nullptr;
}

intern void terminate_audio(cloudwx_ctxt *ctxt) {
    audio_terminate(ctxt->ma);
    audio_destroy(ctxt->ma);
    ctxt->ma = nullptr;
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

    init_work_queue(&ctxt->wq);

    srand((u32)time(NULL));

    if (!init_mongodb(ctxt)) {
        return false;
    }

    if (!init_audio(ctxt)) {
        terminate_mongodb(ctxt);
        return false;
    }

    return true;
}

void terminate_cloudwx(cloudwx_ctxt *ctxt)
{
    ilog("Terminating cloudwx");
    terminate_audio(ctxt);
    terminate_mongodb(ctxt);
}
