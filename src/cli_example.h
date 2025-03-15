#pragma once

/* #include "common.h" */
/* #include "common-whisper.h" */

#include "whisper.h"
// #include "grammar-parser.h"

// #include <cmath>
// #include <fstream>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>
#include <cstring>
#include <map>

namespace grammar_parser
{
struct parse_state
{
    std::map<std::string, uint32_t> symbol_ids;
    std::vector<std::vector<whisper_grammar_element>> rules;

    std::vector<const whisper_grammar_element *> c_rules() const;
};

parse_state parse(const char *src);
void print_grammar(FILE *file, const parse_state &state);
} // namespace grammar_parser

// command-line parameters
struct whisper_params
{
    int32_t n_threads = std::min(4, (int32_t)std::thread::hardware_concurrency());
    int32_t n_processors = 1;
    int32_t offset_t_ms = 0;
    int32_t offset_n = 0;
    int32_t duration_ms = 0;
    int32_t progress_step = 5;
    int32_t max_context = -1;
    int32_t max_len = 0;
    int32_t best_of = whisper_full_default_params(WHISPER_SAMPLING_GREEDY).greedy.best_of;
    int32_t beam_size = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH).beam_search.beam_size;
    int32_t audio_ctx = 0;

    float word_thold = 0.01f;
    float entropy_thold = 2.40f;
    float logprob_thold = -1.00f;
    float no_speech_thold = 0.6f;
    float grammar_penalty = 100.0f;
    float temperature = 0.0f;
    float temperature_inc = 0.2f;

    bool debug_mode = false;
    bool translate = false;
    bool detect_language = false;
    bool diarize = false;
    bool tinydiarize = false;
    bool split_on_word = false;
    bool no_fallback = false;
    bool output_txt = false;
    bool output_vtt = false;
    bool output_srt = false;
    bool output_wts = false;
    bool output_csv = false;
    bool output_jsn = false;
    bool output_jsn_full = false;
    bool output_lrc = false;
    bool no_prints = false;
    bool print_special = false;
    bool print_colors = false;
    bool print_progress = false;
    bool no_timestamps = false;
    bool log_score = false;
    bool use_gpu = true;
    bool flash_attn = false;
    bool suppress_nst = false;

    std::string language = "en";
    std::string prompt;
    std::string font_path = "/System/Library/Fonts/Supplemental/Courier New Bold.ttf";
    std::string model = "models/ggml-base.en.bin";
    std::string grammar;
    std::string grammar_rule;

    // [TDRZ] speaker turn string
    std::string tdrz_speaker_turn = " [SPEAKER_TURN]"; // TODO: set from command line

    // A regular expression that matches tokens to suppress
    std::string suppress_regex;

    std::string openvino_encode_device = "CPU";

    std::string dtw = "";

    std::vector<std::string> fname_inp = {};
    std::vector<std::string> fname_out = {};

    grammar_parser::parse_state grammar_parsed;
};

struct whisper_print_user_data
{
    const whisper_params *params;

    const std::vector<std::vector<float>> *pcmf32s;
    int progress_prev;
};

bool is_file_exist(const char * filename);

void whisper_print_usage(int argc, char **argv, const whisper_params &params);
char *whisper_param_turn_lowercase(char *in);
char *requires_value_error(const std::string &arg);
bool whisper_params_parse(int argc, char **argv, whisper_params &params);
void whisper_print_usage(int /*argc*/, char **argv, const whisper_params &params);
std::string estimate_diarization_speaker(std::vector<std::vector<float>> pcmf32s, int64_t t0, int64_t t1, bool id_only = false);
void whisper_print_progress_callback(struct whisper_context * /*ctx*/, struct whisper_state * /*state*/, int progress, void *user_data);
void whisper_print_segment_callback(struct whisper_context *ctx, struct whisper_state * /*state*/, int n_new, void *user_data);
bool output_txt(struct whisper_context *ctx, const char *fname, const whisper_params &params, std::vector<std::vector<float>> pcmf32s);
bool output_vtt(struct whisper_context *ctx, const char *fname, const whisper_params &params, std::vector<std::vector<float>> pcmf32s);
bool output_srt(struct whisper_context *ctx, const char *fname, const whisper_params &params, std::vector<std::vector<float>> pcmf32s);
char *escape_double_quotes_and_backslashes(const char *str);
// double quote should be escaped by another double quote. (rfc4180)
char *escape_double_quotes_in_csv(const char *str);
bool output_csv(struct whisper_context *ctx, const char *fname, const whisper_params &params, std::vector<std::vector<float>> pcmf32s);
bool output_score(struct whisper_context *ctx,
                  const char *fname,
                  const whisper_params & /*params*/,
                  std::vector<std::vector<float>> /*pcmf32s*/);
bool output_json(struct whisper_context *ctx,
                 const char *fname,
                 const whisper_params &params,
                 std::vector<std::vector<float>> pcmf32s,
                 bool full);

// karaoke video generation
// outputs a bash script that uses ffmpeg to generate a video with the subtitles
// TODO: font parameter adjustments
bool output_wts(struct whisper_context *ctx,
                const char *fname,
                const char *fname_inp,
                const whisper_params &params,
                float t_sec,
                std::vector<std::vector<float>> pcmf32s);
bool output_lrc(struct whisper_context *ctx, const char *fname, const whisper_params &params, std::vector<std::vector<float>> pcmf32s);
void cb_log_disable(enum ggml_log_level, const char *, void *);
bool read_audio_data(const std::string &fname, std::vector<float> &pcmf32, std::vector<std::vector<float>> &pcmf32s, bool stereo);
// convert timestamp to string, 6000 -> 01:00.000
std::string to_timestamp(int64_t t, bool comma = false);
int timestamp_to_sample(int64_t t, int n_samples, int whisper_sample_rate);
int cli_example_main(int argc, char **argv);

