#pragma once
struct audio_ctxt;
struct work_queue;

audio_ctxt *audio_create();
void audio_destroy(audio_ctxt *aud);
bool audio_init(audio_ctxt *aud);
void audio_terminate(audio_ctxt *aud);
void process_available_audio(audio_ctxt *ma, work_queue *wq);
