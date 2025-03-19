#pragma once

struct miniaudio_ctxt;
struct whisper_context;
using whisper_ctxt = whisper_context;

struct cloudwx_ctxt
{
    miniaudio_ctxt *ma;
    whisper_ctxt *whisper;
};

bool init_cloudwx(cloudwx_ctxt *ctxt);
void terminate_cloudwx(cloudwx_ctxt *ctxt);
