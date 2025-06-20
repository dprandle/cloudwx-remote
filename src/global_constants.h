#pragma once
#include "basic_types.h"
inline constexpr s32 AUDIO_CHUNK_DURATION_MS = 200;
inline constexpr s32 AUDIO_ENTRY_MAX_DURATION_S = 60;
inline constexpr s32 APPROXIMATE_SPEECH_CHARS_PER_S = 13;
inline constexpr s32 AUDIO_SAMPLE_RATE = 16000;
inline constexpr f32 AUDIO_SILENT_THRESHOLD_RMS = 0.002f;
inline constexpr u32 AUDIO_CHANNEL_COUNT = 1;
// How much silence do we need to stop recording a chunk and send it over to whisper
inline constexpr sizet CONSECUTIVE_SILENT_AUDIO_THRESHOLD_MS = 2200;
inline constexpr cstr WHISPER_MODEL_FILE = "models/ggml-tiny.en.bin";
inline constexpr sizet WHISPER_CHAR_BUF_SZ = AUDIO_ENTRY_MAX_DURATION_S * APPROXIMATE_SPEECH_CHARS_PER_S * 2 + 1;
