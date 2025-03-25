#pragma once

struct miniaudio_ctxt;
struct whisper_context;
struct whisper_ctxt;

inline constexpr int AUDIO_SAMPLE_RATE = 16000;
inline constexpr float SILENT_THRESHOLD = 0.005f;
inline constexpr const char *WHISPER_MODEL_FILE = "models/ggml-tiny.en.bin";

struct cloudwx_ctxt
{
    miniaudio_ctxt *ma;
    whisper_ctxt *whisper;
};

bool init_cloudwx(cloudwx_ctxt *ctxt);
void terminate_cloudwx(cloudwx_ctxt *ctxt);
void process_available_audio(miniaudio_ctxt *ma, whisper_ctxt *whisper);

