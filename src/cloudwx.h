#pragma once
#include "work_queue.h"

struct audio_ctxt;
struct mongodb_ctxt;
struct raw_metar_entry;

struct cloudwx_ctxt
{
    audio_ctxt *ma;
    mongodb_ctxt *db;
    work_queue wq;
};

bool init_cloudwx(cloudwx_ctxt *ctxt);
void terminate_cloudwx(cloudwx_ctxt *ctxt);
void process_available_audio(audio_ctxt *ma, work_queue *wq);

