#pragma once

#include "basic_types.h"

using namespace nslib;
struct miniaudio_ctxt;
struct whisper_context;
struct whisper_ctxt;

intern constexpr i32 AUDIO_CHUNK_SIZE_MS = 200;
intern constexpr i32 WHISPER_MAX_AUDIO_CHUNK_SIZE_S = 60;
inline constexpr int AUDIO_SAMPLE_RATE = 16000;
inline constexpr float AUDIO_SILENT_THRESHOLD_RMS = 0.001f;
inline constexpr u32 AUDIO_CHANNEL_COUNT = 1;
// How much silence do we need to stop recording a chunk and send it over to whisper
inline constexpr sizet CONSECUTIVE_SILENT_AUDIO_THRESHOLD_MS = 1600;
inline constexpr const char *WHISPER_MODEL_FILE = "models/ggml-tiny.en.bin";

struct cloudwx_ctxt
{
    miniaudio_ctxt *ma;
    whisper_ctxt *whisper;
};

bool init_cloudwx(cloudwx_ctxt *ctxt);
void terminate_cloudwx(cloudwx_ctxt *ctxt);
void process_available_audio(miniaudio_ctxt *ma, whisper_ctxt *whisper);

