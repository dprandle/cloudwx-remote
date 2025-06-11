#pragma once
#include "work_queue.h"

struct miniaudio_ctxt;
struct whisper_context;
struct whisper_ctxt;
struct mongodb_ctxt;
struct raw_metar_entry;

struct cloudwx_ctxt
{
    miniaudio_ctxt *ma;
    whisper_ctxt *whisper;
    mongodb_ctxt *db;
    work_queue wq;
};

bool init_cloudwx(cloudwx_ctxt *ctxt);
void terminate_cloudwx(cloudwx_ctxt *ctxt);
void process_available_audio(miniaudio_ctxt *ma, work_queue *wq);

