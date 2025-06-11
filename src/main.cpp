#include "miniaudio.h"
#include "logging.h"
#include "cloudwx.h"
#include "mongodb.h"

cloudwx_ctxt ctxt{};
int main(int argc, char **argv)
{
    if (!init_cloudwx(&ctxt)) {
        wlog("Failed to initialize cloudwx");
        return -1;
    }
    raw_metar_entry entry{};
    while(1) {
        process_available_audio(ctxt.ma, ctxt.whisper, &entry);
        mongodb_save_item(ctxt.db, "kpns", entry, nullptr);
    }
    ilog("Successfully initialized cloudwx");
    terminate_cloudwx(&ctxt);
}
