#pragma once

#include "basic_types.h"

using namespace nslib;
struct miniaudio_ctxt;
struct whisper_context;
struct whisper_ctxt;

inline constexpr s32 AUDIO_CHUNK_SIZE_MS = 200;
inline constexpr s32 WHISPER_MAX_AUDIO_CHUNK_SIZE_S = 60;
inline constexpr s32 APPROXIMATE_SPEECH_CHARS_PER_S = 13;
inline constexpr s32 AUDIO_SAMPLE_RATE = 16000;
inline constexpr f32 AUDIO_SILENT_THRESHOLD_RMS = 0.002f;
inline constexpr u32 AUDIO_CHANNEL_COUNT = 1;
// How much silence do we need to stop recording a chunk and send it over to whisper
inline constexpr sizet CONSECUTIVE_SILENT_AUDIO_THRESHOLD_MS = 1200;
inline constexpr cstr WHISPER_MODEL_FILE = "models/ggml-tiny.en.bin";

struct cloudwx_ctxt
{
    miniaudio_ctxt *ma;
    whisper_ctxt *whisper;
};

bool init_cloudwx(cloudwx_ctxt *ctxt);
void terminate_cloudwx(cloudwx_ctxt *ctxt);
void process_available_audio(miniaudio_ctxt *ma, whisper_ctxt *whisper);

