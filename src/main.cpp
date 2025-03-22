#include <atomic>
#include "miniaudio.h"
#include "logging.h"
#include "cloudwx.h"

int main(int argc, char **argv)
{
    cloudwx_ctxt ctxt{};
    if (!init_cloudwx(&ctxt)) {
        wlog("Failed to initialize cloudwx");
        return -1;
    }
    while(1) {
        process_available_audio(ctxt.ma, ctxt.whisper);
    }
    ilog("Successfully initialized cloudwx");
    terminate_cloudwx(&ctxt);
}
