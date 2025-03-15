#include "cli_example.h"
#include "miniaudio.h"
#include "whisper.h"
// #include "grammar-parser.h"

#include <cmath>
#include <fstream>
#include <cstdio>
#include <string>
#include <vector>
#include <cstring>

#define SQR(X) ((X) * (X))
#define UNCUBE(x) x < 48 ? 0 : x < 115 ? 1 : (x - 35) / 40

// helper function to replace substrings
static void replace_all(std::string &s, const std::string &search, const std::string &replace)
{
    for (size_t pos = 0;; pos += replace.length()) {
        pos = s.find(search, pos);
        if (pos == std::string::npos)
            break;
        s.erase(pos, search.length());
        s.insert(pos, replace);
    }
}

static int rgb2xterm256(int r, int g, int b)
{
    unsigned char cube[] = {0, 0137, 0207, 0257, 0327, 0377};
    int av, ir, ig, ib, il, qr, qg, qb, ql;
    av = r * .299 + g * .587 + b * .114 + .5;
    ql = (il = av > 238 ? 23 : (av - 3) / 10) * 10 + 8;
    qr = cube[(ir = UNCUBE(r))];
    qg = cube[(ig = UNCUBE(g))];
    qb = cube[(ib = UNCUBE(b))];
    if (SQR(qr - r) + SQR(qg - g) + SQR(qb - b) <= SQR(ql - r) + SQR(ql - g) + SQR(ql - b))
        return ir * 36 + ig * 6 + ib + 020;
    return il + 0350;
}

static std::string set_xterm256_foreground(int r, int g, int b)
{
    int x = rgb2xterm256(r, g, b);
    return "\033[38;5;" + std::to_string(x) + "m";
}

// Lowest is red, middle is yellow, highest is green. Color scheme from
// Paul Tol; it is colorblind friendly https://personal.sron.nl/~pault/
const std::vector<std::string> k_colors = {
    set_xterm256_foreground(220, 5, 12),
    set_xterm256_foreground(232, 96, 28),
    set_xterm256_foreground(241, 147, 45),
    set_xterm256_foreground(246, 193, 65),
    set_xterm256_foreground(247, 240, 86),
    set_xterm256_foreground(144, 201, 135),
    set_xterm256_foreground(78, 178, 101),
};

void whisper_print_usage(int argc, char **argv, const whisper_params &params);

char *whisper_param_turn_lowercase(char *in)
{
    int string_len = strlen(in);
    for (int i = 0; i < string_len; i++) {
        *(in + i) = tolower((unsigned char)*(in + i));
    }
    return in;
}

char *requires_value_error(const std::string &arg)
{
    fprintf(stderr, "error: argument %s requires value\n", arg.c_str());
    exit(0);
}

bool whisper_params_parse(int argc, char **argv, whisper_params &params)
{
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-") {
            params.fname_inp.push_back(arg);
            continue;
        }

        if (arg[0] != '-') {
            params.fname_inp.push_back(arg);
            continue;
        }

        if (arg == "-h" || arg == "--help") {
            whisper_print_usage(argc, argv, params);
            exit(0);
        }
#define ARGV_NEXT (((i + 1) < argc) ? argv[++i] : requires_value_error(arg))
        else if (arg == "-t" || arg == "--threads") {
            params.n_threads = std::stoi(ARGV_NEXT);
        }
        else if (arg == "-p" || arg == "--processors") {
            params.n_processors = std::stoi(ARGV_NEXT);
        }
        else if (arg == "-ot" || arg == "--offset-t") {
            params.offset_t_ms = std::stoi(ARGV_NEXT);
        }
        else if (arg == "-on" || arg == "--offset-n") {
            params.offset_n = std::stoi(ARGV_NEXT);
        }
        else if (arg == "-d" || arg == "--duration") {
            params.duration_ms = std::stoi(ARGV_NEXT);
        }
        else if (arg == "-mc" || arg == "--max-context") {
            params.max_context = std::stoi(ARGV_NEXT);
        }
        else if (arg == "-ml" || arg == "--max-len") {
            params.max_len = std::stoi(ARGV_NEXT);
        }
        else if (arg == "-bo" || arg == "--best-of") {
            params.best_of = std::stoi(ARGV_NEXT);
        }
        else if (arg == "-bs" || arg == "--beam-size") {
            params.beam_size = std::stoi(ARGV_NEXT);
        }
        else if (arg == "-ac" || arg == "--audio-ctx") {
            params.audio_ctx = std::stoi(ARGV_NEXT);
        }
        else if (arg == "-wt" || arg == "--word-thold") {
            params.word_thold = std::stof(ARGV_NEXT);
        }
        else if (arg == "-et" || arg == "--entropy-thold") {
            params.entropy_thold = std::stof(ARGV_NEXT);
        }
        else if (arg == "-lpt" || arg == "--logprob-thold") {
            params.logprob_thold = std::stof(ARGV_NEXT);
        }
        else if (arg == "-nth" || arg == "--no-speech-thold") {
            params.no_speech_thold = std::stof(ARGV_NEXT);
        }
        else if (arg == "-tp" || arg == "--temperature") {
            params.temperature = std::stof(ARGV_NEXT);
        }
        else if (arg == "-tpi" || arg == "--temperature-inc") {
            params.temperature_inc = std::stof(ARGV_NEXT);
        }
        else if (arg == "-debug" || arg == "--debug-mode") {
            params.debug_mode = true;
        }
        else if (arg == "-tr" || arg == "--translate") {
            params.translate = true;
        }
        else if (arg == "-di" || arg == "--diarize") {
            params.diarize = true;
        }
        else if (arg == "-tdrz" || arg == "--tinydiarize") {
            params.tinydiarize = true;
        }
        else if (arg == "-sow" || arg == "--split-on-word") {
            params.split_on_word = true;
        }
        else if (arg == "-nf" || arg == "--no-fallback") {
            params.no_fallback = true;
        }
        else if (arg == "-otxt" || arg == "--output-txt") {
            params.output_txt = true;
        }
        else if (arg == "-ovtt" || arg == "--output-vtt") {
            params.output_vtt = true;
        }
        else if (arg == "-osrt" || arg == "--output-srt") {
            params.output_srt = true;
        }
        else if (arg == "-owts" || arg == "--output-words") {
            params.output_wts = true;
        }
        else if (arg == "-olrc" || arg == "--output-lrc") {
            params.output_lrc = true;
        }
        else if (arg == "-fp" || arg == "--font-path") {
            params.font_path = ARGV_NEXT;
        }
        else if (arg == "-ocsv" || arg == "--output-csv") {
            params.output_csv = true;
        }
        else if (arg == "-oj" || arg == "--output-json") {
            params.output_jsn = true;
        }
        else if (arg == "-ojf" || arg == "--output-json-full") {
            params.output_jsn_full = params.output_jsn = true;
        }
        else if (arg == "-of" || arg == "--output-file") {
            params.fname_out.emplace_back(ARGV_NEXT);
        }
        else if (arg == "-np" || arg == "--no-prints") {
            params.no_prints = true;
        }
        else if (arg == "-ps" || arg == "--print-special") {
            params.print_special = true;
        }
        else if (arg == "-pc" || arg == "--print-colors") {
            params.print_colors = true;
        }
        else if (arg == "-pp" || arg == "--print-progress") {
            params.print_progress = true;
        }
        else if (arg == "-nt" || arg == "--no-timestamps") {
            params.no_timestamps = true;
        }
        else if (arg == "-l" || arg == "--language") {
            params.language = whisper_param_turn_lowercase(ARGV_NEXT);
        }
        else if (arg == "-dl" || arg == "--detect-language") {
            params.detect_language = true;
        }
        else if (arg == "--prompt") {
            params.prompt = ARGV_NEXT;
        }
        else if (arg == "-m" || arg == "--model") {
            params.model = ARGV_NEXT;
        }
        else if (arg == "-f" || arg == "--file") {
            params.fname_inp.emplace_back(ARGV_NEXT);
        }
        else if (arg == "-oved" || arg == "--ov-e-device") {
            params.openvino_encode_device = ARGV_NEXT;
        }
        else if (arg == "-dtw" || arg == "--dtw") {
            params.dtw = ARGV_NEXT;
        }
        else if (arg == "-ls" || arg == "--log-score") {
            params.log_score = true;
        }
        else if (arg == "-ng" || arg == "--no-gpu") {
            params.use_gpu = false;
        }
        else if (arg == "-fa" || arg == "--flash-attn") {
            params.flash_attn = true;
        }
        else if (arg == "-sns" || arg == "--suppress-nst") {
            params.suppress_nst = true;
        }
        else if (arg == "--suppress-regex") {
            params.suppress_regex = ARGV_NEXT;
        }
        else if (arg == "--grammar") {
            params.grammar = ARGV_NEXT;
        }
        else if (arg == "--grammar-rule") {
            params.grammar_rule = ARGV_NEXT;
        }
        else if (arg == "--grammar-penalty") {
            params.grammar_penalty = std::stof(ARGV_NEXT);
        }
        else {
            fprintf(stderr, "error: unknown argument: %s\n", arg.c_str());
            whisper_print_usage(argc, argv, params);
            exit(0);
        }
    }

    return true;
}

void whisper_print_usage(int /*argc*/, char **argv, const whisper_params &params)
{
    fprintf(stderr, "\n");
    fprintf(stderr, "usage: %s [options] file0 file1 ...\n", argv[0]);
    fprintf(stderr, "supported audio formats: flac, mp3, ogg, wav\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, "  -h,        --help              [default] show this help message and exit\n");
    fprintf(stderr, "  -t N,      --threads N         [%-7d] number of threads to use during computation\n", params.n_threads);
    fprintf(stderr, "  -p N,      --processors N      [%-7d] number of processors to use during computation\n", params.n_processors);
    fprintf(stderr, "  -ot N,     --offset-t N        [%-7d] time offset in milliseconds\n", params.offset_t_ms);
    fprintf(stderr, "  -on N,     --offset-n N        [%-7d] segment index offset\n", params.offset_n);
    fprintf(stderr, "  -d  N,     --duration N        [%-7d] duration of audio to process in milliseconds\n", params.duration_ms);
    fprintf(stderr, "  -mc N,     --max-context N     [%-7d] maximum number of text context tokens to store\n", params.max_context);
    fprintf(stderr, "  -ml N,     --max-len N         [%-7d] maximum segment length in characters\n", params.max_len);
    fprintf(stderr, "  -sow,      --split-on-word     [%-7s] split on word rather than on token\n", params.split_on_word ? "true" : "false");
    fprintf(stderr, "  -bo N,     --best-of N         [%-7d] number of best candidates to keep\n", params.best_of);
    fprintf(stderr, "  -bs N,     --beam-size N       [%-7d] beam size for beam search\n", params.beam_size);
    fprintf(stderr, "  -ac N,     --audio-ctx N       [%-7d] audio context size (0 - all)\n", params.audio_ctx);
    fprintf(stderr, "  -wt N,     --word-thold N      [%-7.2f] word timestamp probability threshold\n", params.word_thold);
    fprintf(stderr, "  -et N,     --entropy-thold N   [%-7.2f] entropy threshold for decoder fail\n", params.entropy_thold);
    fprintf(stderr, "  -lpt N,    --logprob-thold N   [%-7.2f] log probability threshold for decoder fail\n", params.logprob_thold);
    fprintf(stderr, "  -nth N,    --no-speech-thold N [%-7.2f] no speech threshold\n", params.no_speech_thold);
    fprintf(stderr, "  -tp,       --temperature N     [%-7.2f] The sampling temperature, between 0 and 1\n", params.temperature);
    fprintf(stderr, "  -tpi,      --temperature-inc N [%-7.2f] The increment of temperature, between 0 and 1\n", params.temperature_inc);
    fprintf(stderr, "  -debug,    --debug-mode        [%-7s] enable debug mode (eg. dump log_mel)\n", params.debug_mode ? "true" : "false");
    fprintf(
        stderr, "  -tr,       --translate         [%-7s] translate from source language to english\n", params.translate ? "true" : "false");
    fprintf(stderr, "  -di,       --diarize           [%-7s] stereo audio diarization\n", params.diarize ? "true" : "false");
    fprintf(stderr,
            "  -tdrz,     --tinydiarize       [%-7s] enable tinydiarize (requires a tdrz model)\n",
            params.tinydiarize ? "true" : "false");
    fprintf(stderr,
            "  -nf,       --no-fallback       [%-7s] do not use temperature fallback while decoding\n",
            params.no_fallback ? "true" : "false");
    fprintf(stderr, "  -otxt,     --output-txt        [%-7s] output result in a text file\n", params.output_txt ? "true" : "false");
    fprintf(stderr, "  -ovtt,     --output-vtt        [%-7s] output result in a vtt file\n", params.output_vtt ? "true" : "false");
    fprintf(stderr, "  -osrt,     --output-srt        [%-7s] output result in a srt file\n", params.output_srt ? "true" : "false");
    fprintf(stderr, "  -olrc,     --output-lrc        [%-7s] output result in a lrc file\n", params.output_lrc ? "true" : "false");
    fprintf(stderr,
            "  -owts,     --output-words      [%-7s] output script for generating karaoke video\n",
            params.output_wts ? "true" : "false");
    fprintf(stderr, "  -fp,       --font-path         [%-7s] path to a monospace font for karaoke video\n", params.font_path.c_str());
    fprintf(stderr, "  -ocsv,     --output-csv        [%-7s] output result in a CSV file\n", params.output_csv ? "true" : "false");
    fprintf(stderr, "  -oj,       --output-json       [%-7s] output result in a JSON file\n", params.output_jsn ? "true" : "false");
    fprintf(stderr,
            "  -ojf,      --output-json-full  [%-7s] include more information in the JSON file\n",
            params.output_jsn_full ? "true" : "false");
    fprintf(stderr, "  -of FNAME, --output-file FNAME [%-7s] output file path (without file extension)\n", "");
    fprintf(stderr,
            "  -np,       --no-prints         [%-7s] do not print anything other than the results\n",
            params.no_prints ? "true" : "false");
    fprintf(stderr, "  -ps,       --print-special     [%-7s] print special tokens\n", params.print_special ? "true" : "false");
    fprintf(stderr, "  -pc,       --print-colors      [%-7s] print colors\n", params.print_colors ? "true" : "false");
    fprintf(stderr, "  -pp,       --print-progress    [%-7s] print progress\n", params.print_progress ? "true" : "false");
    fprintf(stderr, "  -nt,       --no-timestamps     [%-7s] do not print timestamps\n", params.no_timestamps ? "true" : "false");
    fprintf(stderr, "  -l LANG,   --language LANG     [%-7s] spoken language ('auto' for auto-detect)\n", params.language.c_str());
    fprintf(stderr,
            "  -dl,       --detect-language   [%-7s] exit after automatically detecting language\n",
            params.detect_language ? "true" : "false");
    fprintf(stderr, "             --prompt PROMPT     [%-7s] initial prompt (max n_text_ctx/2 tokens)\n", params.prompt.c_str());
    fprintf(stderr, "  -m FNAME,  --model FNAME       [%-7s] model path\n", params.model.c_str());
    fprintf(stderr, "  -f FNAME,  --file FNAME        [%-7s] input audio file path\n", "");
    fprintf(stderr,
            "  -oved D,   --ov-e-device DNAME [%-7s] the OpenVINO device used for encode inference\n",
            params.openvino_encode_device.c_str());
    fprintf(stderr, "  -dtw MODEL --dtw MODEL         [%-7s] compute token-level timestamps\n", params.dtw.c_str());
    fprintf(stderr, "  -ls,       --log-score         [%-7s] log best decoder scores of tokens\n", params.log_score ? "true" : "false");
    fprintf(stderr, "  -ng,       --no-gpu            [%-7s] disable GPU\n", params.use_gpu ? "false" : "true");
    fprintf(stderr, "  -fa,       --flash-attn        [%-7s] flash attention\n", params.flash_attn ? "true" : "false");
    fprintf(stderr, "  -sns,      --suppress-nst      [%-7s] suppress non-speech tokens\n", params.suppress_nst ? "true" : "false");
    fprintf(stderr, "  --suppress-regex REGEX         [%-7s] regular expression matching tokens to suppress\n", params.suppress_regex.c_str());
    fprintf(stderr, "  --grammar GRAMMAR              [%-7s] GBNF grammar to guide decoding\n", params.grammar.c_str());
    fprintf(stderr, "  --grammar-rule RULE            [%-7s] top-level GBNF grammar rule name\n", params.grammar_rule.c_str());
    fprintf(stderr, "  --grammar-penalty N            [%-7.1f] scales down logits of nongrammar tokens\n", params.grammar_penalty);
    fprintf(stderr, "\n");
}

std::string estimate_diarization_speaker(std::vector<std::vector<float>> pcmf32s, int64_t t0, int64_t t1, bool id_only)
{
    std::string speaker = "";
    const int64_t n_samples = pcmf32s[0].size();

    const int64_t is0 = timestamp_to_sample(t0, n_samples, WHISPER_SAMPLE_RATE);
    const int64_t is1 = timestamp_to_sample(t1, n_samples, WHISPER_SAMPLE_RATE);

    double energy0 = 0.0f;
    double energy1 = 0.0f;

    for (int64_t j = is0; j < is1; j++) {
        energy0 += fabs(pcmf32s[0][j]);
        energy1 += fabs(pcmf32s[1][j]);
    }

    if (energy0 > 1.1 * energy1) {
        speaker = "0";
    }
    else if (energy1 > 1.1 * energy0) {
        speaker = "1";
    }
    else {
        speaker = "?";
    }

    // printf("is0 = %lld, is1 = %lld, energy0 = %f, energy1 = %f, speaker = %s\n", is0, is1, energy0, energy1, speaker.c_str());

    if (!id_only) {
        speaker.insert(0, "(speaker ");
        speaker.append(")");
    }

    return speaker;
}

void whisper_print_progress_callback(struct whisper_context * /*ctx*/, struct whisper_state * /*state*/, int progress, void *user_data)
{
    int progress_step = ((whisper_print_user_data *)user_data)->params->progress_step;
    int *progress_prev = &(((whisper_print_user_data *)user_data)->progress_prev);
    if (progress >= *progress_prev + progress_step) {
        *progress_prev += progress_step;
        fprintf(stderr, "%s: progress = %3d%%\n", __func__, progress);
    }
}

void whisper_print_segment_callback(struct whisper_context *ctx, struct whisper_state * /*state*/, int n_new, void *user_data)
{
    const auto &params = *((whisper_print_user_data *)user_data)->params;
    const auto &pcmf32s = *((whisper_print_user_data *)user_data)->pcmf32s;

    const int n_segments = whisper_full_n_segments(ctx);

    std::string speaker = "";

    int64_t t0 = 0;
    int64_t t1 = 0;

    // print the last n_new segments
    const int s0 = n_segments - n_new;

    if (s0 == 0) {
        printf("\n");
    }

    for (int i = s0; i < n_segments; i++) {
        if (!params.no_timestamps || params.diarize) {
            t0 = whisper_full_get_segment_t0(ctx, i);
            t1 = whisper_full_get_segment_t1(ctx, i);
        }

        if (!params.no_timestamps) {
            printf("[%s --> %s]  ", to_timestamp(t0).c_str(), to_timestamp(t1).c_str());
        }

        if (params.diarize && pcmf32s.size() == 2) {
            speaker = estimate_diarization_speaker(pcmf32s, t0, t1);
        }

        if (params.print_colors) {
            for (int j = 0; j < whisper_full_n_tokens(ctx, i); ++j) {
                if (params.print_special == false) {
                    const whisper_token id = whisper_full_get_token_id(ctx, i, j);
                    if (id >= whisper_token_eot(ctx)) {
                        continue;
                    }
                }

                const char *text = whisper_full_get_token_text(ctx, i, j);
                const float p = whisper_full_get_token_p(ctx, i, j);

                const int col = std::max(0, std::min((int)k_colors.size() - 1, (int)(std::pow(p, 3) * float(k_colors.size()))));

                printf("%s%s%s%s", speaker.c_str(), k_colors[col].c_str(), text, "\033[0m");
            }
        }
        else {
            const char *text = whisper_full_get_segment_text(ctx, i);

            printf("%s%s", speaker.c_str(), text);
        }

        if (params.tinydiarize) {
            if (whisper_full_get_segment_speaker_turn_next(ctx, i)) {
                printf("%s", params.tdrz_speaker_turn.c_str());
            }
        }

        // with timestamps or speakers: each segment on new line
        if (!params.no_timestamps || params.diarize) {
            printf("\n");
        }

        fflush(stdout);
    }
}

bool output_txt(struct whisper_context *ctx, const char *fname, const whisper_params &params, std::vector<std::vector<float>> pcmf32s)
{
    std::ofstream fout(fname);
    if (!fout.is_open()) {
        fprintf(stderr, "%s: failed to open '%s' for writing\n", __func__, fname);
        return false;
    }

    fprintf(stderr, "%s: saving output to '%s'\n", __func__, fname);

    const int n_segments = whisper_full_n_segments(ctx);
    for (int i = 0; i < n_segments; ++i) {
        const char *text = whisper_full_get_segment_text(ctx, i);
        std::string speaker = "";

        if (params.diarize && pcmf32s.size() == 2) {
            const int64_t t0 = whisper_full_get_segment_t0(ctx, i);
            const int64_t t1 = whisper_full_get_segment_t1(ctx, i);
            speaker = estimate_diarization_speaker(pcmf32s, t0, t1);
        }

        fout << speaker << text << "\n";
    }

    return true;
}

bool output_vtt(struct whisper_context *ctx, const char *fname, const whisper_params &params, std::vector<std::vector<float>> pcmf32s)
{
    std::ofstream fout(fname);
    if (!fout.is_open()) {
        fprintf(stderr, "%s: failed to open '%s' for writing\n", __func__, fname);
        return false;
    }

    fprintf(stderr, "%s: saving output to '%s'\n", __func__, fname);

    fout << "WEBVTT\n\n";

    const int n_segments = whisper_full_n_segments(ctx);
    for (int i = 0; i < n_segments; ++i) {
        const char *text = whisper_full_get_segment_text(ctx, i);
        const int64_t t0 = whisper_full_get_segment_t0(ctx, i);
        const int64_t t1 = whisper_full_get_segment_t1(ctx, i);
        std::string speaker = "";

        if (params.diarize && pcmf32s.size() == 2) {
            speaker = estimate_diarization_speaker(pcmf32s, t0, t1, true);
            speaker.insert(0, "<v Speaker");
            speaker.append(">");
        }

        fout << to_timestamp(t0) << " --> " << to_timestamp(t1) << "\n";
        fout << speaker << text << "\n\n";
    }

    return true;
}

bool output_srt(struct whisper_context *ctx, const char *fname, const whisper_params &params, std::vector<std::vector<float>> pcmf32s)
{
    std::ofstream fout(fname);
    if (!fout.is_open()) {
        fprintf(stderr, "%s: failed to open '%s' for writing\n", __func__, fname);
        return false;
    }

    fprintf(stderr, "%s: saving output to '%s'\n", __func__, fname);

    const int n_segments = whisper_full_n_segments(ctx);
    for (int i = 0; i < n_segments; ++i) {
        const char *text = whisper_full_get_segment_text(ctx, i);
        const int64_t t0 = whisper_full_get_segment_t0(ctx, i);
        const int64_t t1 = whisper_full_get_segment_t1(ctx, i);
        std::string speaker = "";

        if (params.diarize && pcmf32s.size() == 2) {
            speaker = estimate_diarization_speaker(pcmf32s, t0, t1);
        }

        fout << i + 1 + params.offset_n << "\n";
        fout << to_timestamp(t0, true) << " --> " << to_timestamp(t1, true) << "\n";
        fout << speaker << text << "\n\n";
    }

    return true;
}

char *escape_double_quotes_and_backslashes(const char *str)
{
    if (str == NULL) {
        return NULL;
    }

    size_t escaped_length = strlen(str) + 1;

    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '"' || str[i] == '\\') {
            escaped_length++;
        }
    }

    char *escaped = (char *)calloc(escaped_length, 1); // pre-zeroed
    if (escaped == NULL) {
        return NULL;
    }

    size_t pos = 0;
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '"' || str[i] == '\\') {
            escaped[pos++] = '\\';
        }
        escaped[pos++] = str[i];
    }

    // no need to set zero due to calloc() being used prior

    return escaped;
}

// double quote should be escaped by another double quote. (rfc4180)
char *escape_double_quotes_in_csv(const char *str)
{
    if (str == NULL) {
        return NULL;
    }

    size_t escaped_length = strlen(str) + 1;

    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '"') {
            escaped_length++;
        }
    }

    char *escaped = (char *)calloc(escaped_length, 1); // pre-zeroed
    if (escaped == NULL) {
        return NULL;
    }

    size_t pos = 0;
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '"') {
            escaped[pos++] = '"';
        }
        escaped[pos++] = str[i];
    }

    // no need to set zero due to calloc() being used prior

    return escaped;
}

bool output_csv(struct whisper_context *ctx, const char *fname, const whisper_params &params, std::vector<std::vector<float>> pcmf32s)
{
    std::ofstream fout(fname);
    if (!fout.is_open()) {
        fprintf(stderr, "%s: failed to open '%s' for writing\n", __func__, fname);
        return false;
    }

    fprintf(stderr, "%s: saving output to '%s'\n", __func__, fname);

    const int n_segments = whisper_full_n_segments(ctx);
    fout << "start,end,";
    if (params.diarize && pcmf32s.size() == 2) {
        fout << "speaker,";
    }
    fout << "text\n";

    for (int i = 0; i < n_segments; ++i) {
        const char *text = whisper_full_get_segment_text(ctx, i);
        const int64_t t0 = whisper_full_get_segment_t0(ctx, i);
        const int64_t t1 = whisper_full_get_segment_t1(ctx, i);
        char *text_escaped = escape_double_quotes_in_csv(text);

        // need to multiply times returned from whisper_full_get_segment_t{0,1}() by 10 to get milliseconds.
        fout << 10 * t0 << "," << 10 * t1 << ",";
        if (params.diarize && pcmf32s.size() == 2) {
            fout << estimate_diarization_speaker(pcmf32s, t0, t1, true) << ",";
        }
        fout << "\"" << text_escaped << "\"\n";
    }

    return true;
}

bool output_score(struct whisper_context *ctx, const char *fname, const whisper_params & /*params*/, std::vector<std::vector<float>> /*pcmf32s*/)
{
    std::ofstream fout(fname);
    fprintf(stderr, "%s: saving output to '%s'\n", __func__, fname);

    const int n_segments = whisper_full_n_segments(ctx);
    // fprintf(stderr,"segments: %d\n",n_segments);
    for (int i = 0; i < n_segments; ++i) {
        const int n_tokens = whisper_full_n_tokens(ctx, i);
        // fprintf(stderr,"tokens: %d\n",n_tokens);
        for (int j = 0; j < n_tokens; j++) {
            auto token = whisper_full_get_token_text(ctx, i, j);
            auto probability = whisper_full_get_token_p(ctx, i, j);
            fout << token << '\t' << probability << std::endl;
            // fprintf(stderr,"token: %s %f\n",token,probability);
        }
    }
    return true;
}

bool output_json(struct whisper_context *ctx, const char *fname, const whisper_params &params, std::vector<std::vector<float>> pcmf32s, bool full)
{
    std::ofstream fout(fname);
    int indent = 0;

    auto doindent = [&]() {
        for (int i = 0; i < indent; i++)
            fout << "\t";
    };

    auto start_arr = [&](const char *name) {
        doindent();
        fout << "\"" << name << "\": [\n";
        indent++;
    };

    auto end_arr = [&](bool end) {
        indent--;
        doindent();
        fout << (end ? "]\n" : "],\n");
    };

    auto start_obj = [&](const char *name) {
        doindent();
        if (name) {
            fout << "\"" << name << "\": {\n";
        }
        else {
            fout << "{\n";
        }
        indent++;
    };

    auto end_obj = [&](bool end) {
        indent--;
        doindent();
        fout << (end ? "}\n" : "},\n");
    };

    auto start_value = [&](const char *name) {
        doindent();
        fout << "\"" << name << "\": ";
    };

    auto value_s = [&](const char *name, const char *val, bool end) {
        start_value(name);
        char *val_escaped = escape_double_quotes_and_backslashes(val);
        fout << "\"" << val_escaped << (end ? "\"\n" : "\",\n");
        free(val_escaped);
    };

    auto end_value = [&](bool end) { fout << (end ? "\n" : ",\n"); };

    auto value_i = [&](const char *name, const int64_t val, bool end) {
        start_value(name);
        fout << val;
        end_value(end);
    };

    auto value_f = [&](const char *name, const float val, bool end) {
        start_value(name);
        fout << val;
        end_value(end);
    };

    auto value_b = [&](const char *name, const bool val, bool end) {
        start_value(name);
        fout << (val ? "true" : "false");
        end_value(end);
    };

    auto times_o = [&](int64_t t0, int64_t t1, bool end) {
        start_obj("timestamps");
        value_s("from", to_timestamp(t0, true).c_str(), false);
        value_s("to", to_timestamp(t1, true).c_str(), true);
        end_obj(false);
        start_obj("offsets");
        value_i("from", t0 * 10, false);
        value_i("to", t1 * 10, true);
        end_obj(end);
    };

    if (!fout.is_open()) {
        fprintf(stderr, "%s: failed to open '%s' for writing\n", __func__, fname);
        return false;
    }

    fprintf(stderr, "%s: saving output to '%s'\n", __func__, fname);
    start_obj(nullptr);
    value_s("systeminfo", whisper_print_system_info(), false);
    start_obj("model");
    value_s("type", whisper_model_type_readable(ctx), false);
    value_b("multilingual", whisper_is_multilingual(ctx), false);
    value_i("vocab", whisper_model_n_vocab(ctx), false);
    start_obj("audio");
    value_i("ctx", whisper_model_n_audio_ctx(ctx), false);
    value_i("state", whisper_model_n_audio_state(ctx), false);
    value_i("head", whisper_model_n_audio_head(ctx), false);
    value_i("layer", whisper_model_n_audio_layer(ctx), true);
    end_obj(false);
    start_obj("text");
    value_i("ctx", whisper_model_n_text_ctx(ctx), false);
    value_i("state", whisper_model_n_text_state(ctx), false);
    value_i("head", whisper_model_n_text_head(ctx), false);
    value_i("layer", whisper_model_n_text_layer(ctx), true);
    end_obj(false);
    value_i("mels", whisper_model_n_mels(ctx), false);
    value_i("ftype", whisper_model_ftype(ctx), true);
    end_obj(false);
    start_obj("params");
    value_s("model", params.model.c_str(), false);
    value_s("language", params.language.c_str(), false);
    value_b("translate", params.translate, true);
    end_obj(false);
    start_obj("result");
    value_s("language", whisper_lang_str(whisper_full_lang_id(ctx)), true);
    end_obj(false);
    start_arr("transcription");

    const int n_segments = whisper_full_n_segments(ctx);
    for (int i = 0; i < n_segments; ++i) {
        const char *text = whisper_full_get_segment_text(ctx, i);

        const int64_t t0 = whisper_full_get_segment_t0(ctx, i);
        const int64_t t1 = whisper_full_get_segment_t1(ctx, i);

        start_obj(nullptr);
        times_o(t0, t1, false);
        value_s("text", text, !params.diarize && !params.tinydiarize && !full);

        if (full) {
            start_arr("tokens");
            const int n = whisper_full_n_tokens(ctx, i);
            for (int j = 0; j < n; ++j) {
                auto token = whisper_full_get_token_data(ctx, i, j);
                start_obj(nullptr);
                value_s("text", whisper_token_to_str(ctx, token.id), false);
                if (token.t0 > -1 && token.t1 > -1) {
                    // If we have per-token timestamps, write them out
                    times_o(token.t0, token.t1, false);
                }
                value_i("id", token.id, false);
                value_f("p", token.p, false);
                value_f("t_dtw", token.t_dtw, true);
                end_obj(j == (n - 1));
            }
            end_arr(!params.diarize && !params.tinydiarize);
        }

        if (params.diarize && pcmf32s.size() == 2) {
            value_s("speaker", estimate_diarization_speaker(pcmf32s, t0, t1, true).c_str(), true);
        }

        if (params.tinydiarize) {
            value_b("speaker_turn_next", whisper_full_get_segment_speaker_turn_next(ctx, i), true);
        }
        end_obj(i == (n_segments - 1));
    }

    end_arr(true);
    end_obj(true);
    return true;
}

// karaoke video generation
// outputs a bash script that uses ffmpeg to generate a video with the subtitles
// TODO: font parameter adjustments
bool output_wts(struct whisper_context *ctx,
                const char *fname,
                const char *fname_inp,
                const whisper_params &params,
                float t_sec,
                std::vector<std::vector<float>> pcmf32s)
{
    std::ofstream fout(fname);

    fprintf(stderr, "%s: saving output to '%s'\n", __func__, fname);

    static const char *font = params.font_path.c_str();

    std::ifstream fin(font);
    if (!fin.is_open()) {
        fprintf(stderr, "%s: font not found at '%s', please specify a monospace font with -fp\n", __func__, font);
        return false;
    }

    fout << "#!/bin/bash" << "\n";
    fout << "\n";

    fout << "ffmpeg -i " << fname_inp << " -f lavfi -i color=size=1200x120:duration=" << t_sec << ":rate=25:color=black -vf \"";

    for (int i = 0; i < whisper_full_n_segments(ctx); i++) {
        const int64_t t0 = whisper_full_get_segment_t0(ctx, i);
        const int64_t t1 = whisper_full_get_segment_t1(ctx, i);

        const int n = whisper_full_n_tokens(ctx, i);

        std::vector<whisper_token_data> tokens(n);
        for (int j = 0; j < n; ++j) {
            tokens[j] = whisper_full_get_token_data(ctx, i, j);
        }

        if (i > 0) {
            fout << ",";
        }

        // background text
        fout << "drawtext=fontfile='" << font << "':fontsize=24:fontcolor=gray:x=(w-text_w)/2:y=h/2:text='':enable='between(t,"
             << t0 / 100.0 << "," << t0 / 100.0 << ")'";

        bool is_first = true;
        std::string speaker = "";

        if (params.diarize && pcmf32s.size() == 2) {
            speaker = estimate_diarization_speaker(pcmf32s, t0, t1);
        }

        for (int j = 0; j < n; ++j) {
            const auto &token = tokens[j];

            if (tokens[j].id >= whisper_token_eot(ctx)) {
                continue;
            }

            std::string txt_bg = "";
            std::string txt_fg = ""; // highlight token
            std::string txt_ul = ""; // underline

            if (params.diarize && pcmf32s.size() == 2) {
                txt_bg = speaker;
                txt_fg = speaker;
                txt_ul = "\\ \\ \\ \\ \\ \\ \\ \\ \\ \\ \\ ";
            }

            txt_bg.append("> ");
            txt_fg.append("> ");
            txt_ul.append("\\ \\ ");

            {
                for (int k = 0; k < n; ++k) {
                    const auto &token2 = tokens[k];

                    if (tokens[k].id >= whisper_token_eot(ctx)) {
                        continue;
                    }

                    const std::string txt = whisper_token_to_str(ctx, token2.id);

                    txt_bg += txt;

                    if (k == j) {
                        for (int l = 0; l < (int)txt.size(); ++l) {
                            txt_fg += txt[l];
                            txt_ul += "_";
                        }
                        txt_fg += "|";
                    }
                    else {
                        for (int l = 0; l < (int)txt.size(); ++l) {
                            txt_fg += "\\ ";
                            txt_ul += "\\ ";
                        }
                    }
                }

                ::replace_all(txt_bg, "'", "\u2019");
                ::replace_all(txt_bg, "\"", "\\\"");
                ::replace_all(txt_fg, "'", "\u2019");
                ::replace_all(txt_fg, "\"", "\\\"");
            }

            if (is_first) {
                // background text
                fout << ",drawtext=fontfile='" << font << "':fontsize=24:fontcolor=gray:x=(w-text_w)/2:y=h/2:text='" << txt_bg
                     << "':enable='between(t," << t0 / 100.0 << "," << t1 / 100.0 << ")'";
                is_first = false;
            }

            // foreground text
            fout << ",drawtext=fontfile='" << font << "':fontsize=24:fontcolor=lightgreen:x=(w-text_w)/2+8:y=h/2:text='" << txt_fg
                 << "':enable='between(t," << token.t0 / 100.0 << "," << token.t1 / 100.0 << ")'";

            // underline
            fout << ",drawtext=fontfile='" << font << "':fontsize=24:fontcolor=lightgreen:x=(w-text_w)/2+8:y=h/2+16:text='" << txt_ul
                 << "':enable='between(t," << token.t0 / 100.0 << "," << token.t1 / 100.0 << ")'";
        }
    }

    fout << "\" -c:v libx264 -pix_fmt yuv420p -y " << fname_inp << ".mp4" << "\n";

    fout << "\n\n";
    fout << "echo \"Your video has been saved to " << fname_inp << ".mp4\"" << "\n";
    fout << "\n";
    fout << "echo \"  ffplay " << fname_inp << ".mp4\"\n";
    fout << "\n";

    fout.close();

    fprintf(stderr, "%s: run 'source %s' to generate karaoke video\n", __func__, fname);

    return true;
}

bool output_lrc(struct whisper_context *ctx, const char *fname, const whisper_params &params, std::vector<std::vector<float>> pcmf32s)
{
    std::ofstream fout(fname);
    if (!fout.is_open()) {
        fprintf(stderr, "%s: failed to open '%s' for writing\n", __func__, fname);
        return false;
    }

    fprintf(stderr, "%s: saving output to '%s'\n", __func__, fname);

    fout << "[by:whisper.cpp]\n";

    const int n_segments = whisper_full_n_segments(ctx);
    for (int i = 0; i < n_segments; ++i) {
        const char *text = whisper_full_get_segment_text(ctx, i);
        const int64_t t = whisper_full_get_segment_t0(ctx, i);

        int64_t msec = t * 10;
        int64_t min = msec / (1000 * 60);
        msec = msec - min * (1000 * 60);
        int64_t sec = msec / 1000;
        msec = msec - sec * 1000;

        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d.%02d", (int)min, (int)sec, (int)(msec / 10));
        std::string timestamp_lrc = std::string(buf);
        std::string speaker = "";

        if (params.diarize && pcmf32s.size() == 2) {
            const int64_t t0 = whisper_full_get_segment_t0(ctx, i);
            const int64_t t1 = whisper_full_get_segment_t1(ctx, i);
            speaker = estimate_diarization_speaker(pcmf32s, t0, t1);
        }

        fout << '[' << timestamp_lrc << ']' << speaker << text << "\n";
    }

    return true;
}

void cb_log_disable(enum ggml_log_level, const char *, void *)
{}

bool is_file_exist(const char *filename)
{
    std::ifstream infile(filename);
    return infile.good();
}

bool read_audio_data(const std::string &fname, std::vector<float> &pcmf32, std::vector<std::vector<float>> &pcmf32s, bool stereo)
{
    std::vector<uint8_t> audio_data; // used for pipe input from stdin or ffmpeg decoding output

    ma_result result;
    ma_decoder_config decoder_config;
    ma_decoder decoder;

    decoder_config = ma_decoder_config_init(ma_format_f32, stereo ? 2 : 1, WHISPER_SAMPLE_RATE);

    if (fname == "-") {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
#endif

        uint8_t buf[1024];
        while (true) {
            const size_t n = fread(buf, 1, sizeof(buf), stdin);
            if (n == 0) {
                break;
            }
            audio_data.insert(audio_data.end(), buf, buf + n);
        }

        if ((result = ma_decoder_init_memory(audio_data.data(), audio_data.size(), &decoder_config, &decoder)) != MA_SUCCESS) {

            fprintf(stderr, "Error: failed to open audio data from stdin (%s)\n", ma_result_description(result));

            return false;
        }

        fprintf(stderr, "%s: read %zu bytes from stdin\n", __func__, audio_data.size());
    }
    else if (((result = ma_decoder_init_file(fname.c_str(), &decoder_config, &decoder)) != MA_SUCCESS)) {
#if defined(WHISPER_FFMPEG)
        if (ffmpeg_decode_audio(fname, audio_data) != 0) {
            fprintf(stderr, "error: failed to ffmpeg decode '%s'\n", fname.c_str());

            return false;
        }

        if ((result = ma_decoder_init_memory(audio_data.data(), audio_data.size(), &decoder_config, &decoder)) != MA_SUCCESS) {
            fprintf(stderr, "error: failed to read audio data as wav (%s)\n", ma_result_description(result));

            return false;
        }
#else
        if ((result = ma_decoder_init_memory(fname.c_str(), fname.size(), &decoder_config, &decoder)) != MA_SUCCESS) {
            fprintf(stderr, "error: failed to read audio data as wav (%s)\n", ma_result_description(result));

            return false;
        }
#endif
    }

    ma_uint64 frame_count;
    ma_uint64 frames_read;

    if ((result = ma_decoder_get_length_in_pcm_frames(&decoder, &frame_count)) != MA_SUCCESS) {
        fprintf(stderr, "error: failed to retrieve the length of the audio data (%s)\n", ma_result_description(result));

        return false;
    }

    pcmf32.resize(stereo ? frame_count * 2 : frame_count);

    if ((result = ma_decoder_read_pcm_frames(&decoder, pcmf32.data(), frame_count, &frames_read)) != MA_SUCCESS) {
        fprintf(stderr, "error: failed to read the frames of the audio data (%s)\n", ma_result_description(result));

        return false;
    }

    if (stereo) {
        pcmf32s.resize(2);
        pcmf32s[0].resize(frame_count);
        pcmf32s[1].resize(frame_count);
        for (uint64_t i = 0; i < frame_count; i++) {
            pcmf32s[0][i] = pcmf32[2 * i];
            pcmf32s[1][i] = pcmf32[2 * i + 1];
        }
    }

    ma_decoder_uninit(&decoder);

    return true;
}

//  500 -> 00:05.000
// 6000 -> 01:00.000
std::string to_timestamp(int64_t t, bool comma)
{
    int64_t msec = t * 10;
    int64_t hr = msec / (1000 * 60 * 60);
    msec = msec - hr * (1000 * 60 * 60);
    int64_t min = msec / (1000 * 60);
    msec = msec - min * (1000 * 60);
    int64_t sec = msec / 1000;
    msec = msec - sec * 1000;

    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d%s%03d", (int)hr, (int)min, (int)sec, comma ? "," : ".", (int)msec);

    return std::string(buf);
}

int timestamp_to_sample(int64_t t, int n_samples, int whisper_sample_rate)
{
    return std::max(0, std::min((int)n_samples - 1, (int)((t * whisper_sample_rate) / 100)));
}

namespace grammar_parser
{
// NOTE: assumes valid utf8 (but checks for overrun)
// copied from whisper.cpp
static std::pair<uint32_t, const char *> decode_utf8(const char *src)
{
    static const int lookup[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 3, 4};
    uint8_t first_byte = static_cast<uint8_t>(*src);
    uint8_t highbits = first_byte >> 4;
    int len = lookup[highbits];
    uint8_t mask = (1 << (8 - len)) - 1;
    uint32_t value = first_byte & mask;
    const char *end = src + len; // may overrun!
    const char *pos = src + 1;
    for (; pos < end && *pos; pos++) {
        value = (value << 6) + (static_cast<uint8_t>(*pos) & 0x3F);
    }
    return std::make_pair(value, pos);
}

static uint32_t get_symbol_id(parse_state &state, const char *src, size_t len)
{
    uint32_t next_id = static_cast<uint32_t>(state.symbol_ids.size());
    auto result = state.symbol_ids.insert(std::make_pair(std::string(src, len), next_id));
    return result.first->second;
}

static uint32_t generate_symbol_id(parse_state &state, const std::string &base_name)
{
    uint32_t next_id = static_cast<uint32_t>(state.symbol_ids.size());
    state.symbol_ids[base_name + '_' + std::to_string(next_id)] = next_id;
    return next_id;
}

static void add_rule(parse_state &state, uint32_t rule_id, const std::vector<whisper_grammar_element> &rule)
{
    if (state.rules.size() <= rule_id) {
        state.rules.resize(rule_id + 1);
    }
    state.rules[rule_id] = rule;
}

static bool is_word_char(char c)
{
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '-' || ('0' <= c && c <= '9');
}

static std::pair<uint32_t, const char *> parse_hex(const char *src, int size)
{
    const char *pos = src;
    const char *end = src + size;
    uint32_t value = 0;
    for (; pos < end && *pos; pos++) {
        value <<= 4;
        char c = *pos;
        if ('a' <= c && c <= 'f') {
            value += c - 'a' + 10;
        }
        else if ('A' <= c && c <= 'F') {
            value += c - 'A' + 10;
        }
        else if ('0' <= c && c <= '9') {
            value += c - '0';
        }
        else {
            break;
        }
    }
    if (pos != end) {
        throw std::runtime_error("expecting " + std::to_string(size) + " hex chars at " + src);
    }
    return std::make_pair(value, pos);
}

static const char *parse_space(const char *src, bool newline_ok)
{
    const char *pos = src;
    while (*pos == ' ' || *pos == '\t' || *pos == '#' || (newline_ok && (*pos == '\r' || *pos == '\n'))) {
        if (*pos == '#') {
            while (*pos && *pos != '\r' && *pos != '\n') {
                pos++;
            }
        }
        else {
            pos++;
        }
    }
    return pos;
}

static const char *parse_name(const char *src)
{
    const char *pos = src;
    while (is_word_char(*pos)) {
        pos++;
    }
    if (pos == src) {
        throw std::runtime_error(std::string("expecting name at ") + src);
    }
    return pos;
}

static std::pair<uint32_t, const char *> parse_char(const char *src)
{
    if (*src == '\\') {
        switch (src[1]) {
        case 'x':
            return parse_hex(src + 2, 2);
        case 'u':
            return parse_hex(src + 2, 4);
        case 'U':
            return parse_hex(src + 2, 8);
        case 't':
            return std::make_pair('\t', src + 2);
        case 'r':
            return std::make_pair('\r', src + 2);
        case 'n':
            return std::make_pair('\n', src + 2);
        case '\\':
        case '"':
        case '[':
        case ']':
            return std::make_pair(src[1], src + 2);
        default:
            throw std::runtime_error(std::string("unknown escape at ") + src);
        }
    }
    else if (*src) {
        return decode_utf8(src);
    }
    throw std::runtime_error("unexpected end of input");
}

static const char *parse_alternates(parse_state &state, const char *src, const std::string &rule_name, uint32_t rule_id, bool is_nested);

static const char *parse_sequence(parse_state &state,
                                  const char *src,
                                  const std::string &rule_name,
                                  std::vector<whisper_grammar_element> &out_elements,
                                  bool is_nested)
{
    size_t last_sym_start = out_elements.size();
    const char *pos = src;
    while (*pos) {
        if (*pos == '"') { // literal string
            pos++;
            last_sym_start = out_elements.size();
            while (*pos != '"') {
                auto char_pair = parse_char(pos);
                pos = char_pair.second;
                out_elements.push_back({WHISPER_GRETYPE_CHAR, char_pair.first});
            }
            pos = parse_space(pos + 1, is_nested);
        }
        else if (*pos == '[') { // char range(s)
            pos++;
            enum whisper_gretype start_type = WHISPER_GRETYPE_CHAR;
            if (*pos == '^') {
                pos++;
                start_type = WHISPER_GRETYPE_CHAR_NOT;
            }
            last_sym_start = out_elements.size();
            while (*pos != ']') {
                auto char_pair = parse_char(pos);
                pos = char_pair.second;
                enum whisper_gretype type = last_sym_start < out_elements.size() ? WHISPER_GRETYPE_CHAR_ALT : start_type;

                out_elements.push_back({type, char_pair.first});
                if (pos[0] == '-' && pos[1] != ']') {
                    auto endchar_pair = parse_char(pos + 1);
                    pos = endchar_pair.second;
                    out_elements.push_back({WHISPER_GRETYPE_CHAR_RNG_UPPER, endchar_pair.first});
                }
            }
            pos = parse_space(pos + 1, is_nested);
        }
        else if (is_word_char(*pos)) { // rule reference
            const char *name_end = parse_name(pos);
            uint32_t ref_rule_id = get_symbol_id(state, pos, name_end - pos);
            pos = parse_space(name_end, is_nested);
            last_sym_start = out_elements.size();
            out_elements.push_back({WHISPER_GRETYPE_RULE_REF, ref_rule_id});
        }
        else if (*pos == '(') { // grouping
            // parse nested alternates into synthesized rule
            pos = parse_space(pos + 1, true);
            uint32_t sub_rule_id = generate_symbol_id(state, rule_name);
            pos = parse_alternates(state, pos, rule_name, sub_rule_id, true);
            last_sym_start = out_elements.size();
            // output reference to synthesized rule
            out_elements.push_back({WHISPER_GRETYPE_RULE_REF, sub_rule_id});
            if (*pos != ')') {
                throw std::runtime_error(std::string("expecting ')' at ") + pos);
            }
            pos = parse_space(pos + 1, is_nested);
        }
        else if (*pos == '*' || *pos == '+' || *pos == '?') { // repetition operator
            if (last_sym_start == out_elements.size()) {
                throw std::runtime_error(std::string("expecting preceding item to */+/? at ") + pos);
            }

            // apply transformation to previous symbol (last_sym_start to end) according to
            // rewrite rules:
            // S* --> S' ::= S S' |
            // S+ --> S' ::= S S' | S
            // S? --> S' ::= S |
            uint32_t sub_rule_id = generate_symbol_id(state, rule_name);
            std::vector<whisper_grammar_element> sub_rule;
            // add preceding symbol to generated rule
            sub_rule.insert(sub_rule.end(), out_elements.begin() + last_sym_start, out_elements.end());
            if (*pos == '*' || *pos == '+') {
                // cause generated rule to recurse
                sub_rule.push_back({WHISPER_GRETYPE_RULE_REF, sub_rule_id});
            }
            // mark start of alternate def
            sub_rule.push_back({WHISPER_GRETYPE_ALT, 0});
            if (*pos == '+') {
                // add preceding symbol as alternate only for '+' (otherwise empty)
                sub_rule.insert(sub_rule.end(), out_elements.begin() + last_sym_start, out_elements.end());
            }
            sub_rule.push_back({WHISPER_GRETYPE_END, 0});
            add_rule(state, sub_rule_id, sub_rule);

            // in original rule, replace previous symbol with reference to generated rule
            out_elements.resize(last_sym_start);
            out_elements.push_back({WHISPER_GRETYPE_RULE_REF, sub_rule_id});

            pos = parse_space(pos + 1, is_nested);
        }
        else {
            break;
        }
    }
    return pos;
}

static const char *parse_alternates(parse_state &state, const char *src, const std::string &rule_name, uint32_t rule_id, bool is_nested)
{
    std::vector<whisper_grammar_element> rule;
    const char *pos = parse_sequence(state, src, rule_name, rule, is_nested);
    while (*pos == '|') {
        rule.push_back({WHISPER_GRETYPE_ALT, 0});
        pos = parse_space(pos + 1, true);
        pos = parse_sequence(state, pos, rule_name, rule, is_nested);
    }
    rule.push_back({WHISPER_GRETYPE_END, 0});
    add_rule(state, rule_id, rule);
    return pos;
}

static const char *parse_rule(parse_state &state, const char *src)
{
    const char *name_end = parse_name(src);
    const char *pos = parse_space(name_end, false);
    size_t name_len = name_end - src;
    uint32_t rule_id = get_symbol_id(state, src, name_len);
    const std::string name(src, name_len);

    if (!(pos[0] == ':' && pos[1] == ':' && pos[2] == '=')) {
        throw std::runtime_error(std::string("expecting ::= at ") + pos);
    }
    pos = parse_space(pos + 3, true);

    pos = parse_alternates(state, pos, name, rule_id, false);

    if (*pos == '\r') {
        pos += pos[1] == '\n' ? 2 : 1;
    }
    else if (*pos == '\n') {
        pos++;
    }
    else if (*pos) {
        throw std::runtime_error(std::string("expecting newline or end at ") + pos);
    }
    return parse_space(pos, true);
}

parse_state parse(const char *src)
{
    try {
        parse_state state;
        const char *pos = parse_space(src, true);
        while (*pos) {
            pos = parse_rule(state, pos);
        }
        return state;
    }
    catch (const std::exception &err) {
        fprintf(stderr, "%s: error parsing grammar: %s\n", __func__, err.what());
        return parse_state();
    }
}

static void print_grammar_char(FILE *file, uint32_t c)
{
    if (0x20 <= c && c <= 0x7f) {
        fprintf(file, "%c", static_cast<char>(c));
    }
    else {
        // cop out of encoding UTF-8
        fprintf(file, "<U+%04X>", c);
    }
}

static bool is_char_element(whisper_grammar_element elem)
{
    switch (elem.type) {
    case WHISPER_GRETYPE_CHAR:
        return true;
    case WHISPER_GRETYPE_CHAR_NOT:
        return true;
    case WHISPER_GRETYPE_CHAR_ALT:
        return true;
    case WHISPER_GRETYPE_CHAR_RNG_UPPER:
        return true;
    default:
        return false;
    }
}

static void print_rule_binary(FILE *file, const std::vector<whisper_grammar_element> &rule)
{
    for (auto elem : rule) {
        switch (elem.type) {
        case WHISPER_GRETYPE_END:
            fprintf(file, "END");
            break;
        case WHISPER_GRETYPE_ALT:
            fprintf(file, "ALT");
            break;
        case WHISPER_GRETYPE_RULE_REF:
            fprintf(file, "RULE_REF");
            break;
        case WHISPER_GRETYPE_CHAR:
            fprintf(file, "CHAR");
            break;
        case WHISPER_GRETYPE_CHAR_NOT:
            fprintf(file, "CHAR_NOT");
            break;
        case WHISPER_GRETYPE_CHAR_RNG_UPPER:
            fprintf(file, "CHAR_RNG_UPPER");
            break;
        case WHISPER_GRETYPE_CHAR_ALT:
            fprintf(file, "CHAR_ALT");
            break;
        }
        switch (elem.type) {
        case WHISPER_GRETYPE_END:
        case WHISPER_GRETYPE_ALT:
        case WHISPER_GRETYPE_RULE_REF:
            fprintf(file, "(%u) ", elem.value);
            break;
        case WHISPER_GRETYPE_CHAR:
        case WHISPER_GRETYPE_CHAR_NOT:
        case WHISPER_GRETYPE_CHAR_RNG_UPPER:
        case WHISPER_GRETYPE_CHAR_ALT:
            fprintf(file, "(\"");
            print_grammar_char(file, elem.value);
            fprintf(file, "\") ");
            break;
        }
    }
    fprintf(file, "\n");
}

static void print_rule(FILE *file,
                       uint32_t rule_id,
                       const std::vector<whisper_grammar_element> &rule,
                       const std::map<uint32_t, std::string> &symbol_id_names)
{
    if (rule.empty() || rule.back().type != WHISPER_GRETYPE_END) {
        throw std::runtime_error("malformed rule, does not end with WHISPER_GRETYPE_END: " + std::to_string(rule_id));
    }
    fprintf(file, "%s ::= ", symbol_id_names.at(rule_id).c_str());
    for (size_t i = 0, end = rule.size() - 1; i < end; i++) {
        whisper_grammar_element elem = rule[i];
        switch (elem.type) {
        case WHISPER_GRETYPE_END:
            throw std::runtime_error("unexpected end of rule: " + std::to_string(rule_id) + "," + std::to_string(i));
        case WHISPER_GRETYPE_ALT:
            fprintf(file, "| ");
            break;
        case WHISPER_GRETYPE_RULE_REF:
            fprintf(file, "%s ", symbol_id_names.at(elem.value).c_str());
            break;
        case WHISPER_GRETYPE_CHAR:
            fprintf(file, "[");
            print_grammar_char(file, elem.value);
            break;
        case WHISPER_GRETYPE_CHAR_NOT:
            fprintf(file, "[^");
            print_grammar_char(file, elem.value);
            break;
        case WHISPER_GRETYPE_CHAR_RNG_UPPER:
            if (i == 0 || !is_char_element(rule[i - 1])) {
                throw std::runtime_error("WHISPER_GRETYPE_CHAR_RNG_UPPER without preceding char: " + std::to_string(rule_id) + "," +
                                         std::to_string(i));
            }
            fprintf(file, "-");
            print_grammar_char(file, elem.value);
            break;
        case WHISPER_GRETYPE_CHAR_ALT:
            if (i == 0 || !is_char_element(rule[i - 1])) {
                throw std::runtime_error("WHISPER_GRETYPE_CHAR_ALT without preceding char: " + std::to_string(rule_id) + "," +
                                         std::to_string(i));
            }
            print_grammar_char(file, elem.value);
            break;
        }
        if (is_char_element(elem)) {
            switch (rule[i + 1].type) {
            case WHISPER_GRETYPE_CHAR_ALT:
            case WHISPER_GRETYPE_CHAR_RNG_UPPER:
                break;
            default:
                fprintf(file, "] ");
            }
        }
    }
    fprintf(file, "\n");
}

void print_grammar(FILE *file, const parse_state &state)
{
    try {
        std::map<uint32_t, std::string> symbol_id_names;
        for (auto kv : state.symbol_ids) {
            symbol_id_names[kv.second] = kv.first;
        }
        for (size_t i = 0, end = state.rules.size(); i < end; i++) {
            // fprintf(file, "%zu: ", i);
            // print_rule_binary(file, state.rules[i]);
            print_rule(file, uint32_t(i), state.rules[i], symbol_id_names);
            // fprintf(file, "\n");
        }
    }
    catch (const std::exception &err) {
        fprintf(stderr, "\n%s: error printing grammar: %s\n", __func__, err.what());
    }
}

std::vector<const whisper_grammar_element *> parse_state::c_rules() const
{
    std::vector<const whisper_grammar_element *> ret;
    for (const auto &rule : rules) {
        ret.push_back(rule.data());
    }
    return ret;
}

} // namespace grammar_parser

int cli_example_main(int argc, char **argv)
{
    whisper_params params;

    // If the only argument starts with "@", read arguments line-by-line
    // from the given file.
    std::vector<std::string> vec_args;
    if (argc == 2 && argv != nullptr && argv[1] != nullptr && argv[1][0] == '@') {
        // Save the name of the executable.
        vec_args.push_back(argv[0]);

        // Open the response file.
        char const *rspfile = argv[1] + sizeof(char);
        std::ifstream fin(rspfile);
        if (fin.is_open() == false) {
            fprintf(stderr, "error: response file '%s' not found\n", rspfile);
            return 1;
        }

        // Read the entire response file.
        std::string line;
        while (std::getline(fin, line)) {
            vec_args.push_back(line);
        }

        // Use the contents of the response file as the command-line arguments.
        argc = static_cast<int>(vec_args.size());
        argv = static_cast<char **>(alloca(argc * sizeof(char *)));
        for (int i = 0; i < argc; ++i) {
            argv[i] = const_cast<char *>(vec_args[i].c_str());
        }
    }

    if (whisper_params_parse(argc, argv, params) == false) {
        whisper_print_usage(argc, argv, params);
        return 1;
    }

    // remove non-existent files
    for (auto it = params.fname_inp.begin(); it != params.fname_inp.end();) {
        const auto fname_inp = it->c_str();

        if (*it != "-" && !is_file_exist(fname_inp)) {
            fprintf(stderr, "error: input file not found '%s'\n", fname_inp);
            it = params.fname_inp.erase(it);
            continue;
        }

        it++;
    }

    if (params.fname_inp.empty()) {
        fprintf(stderr, "error: no input files specified\n");
        whisper_print_usage(argc, argv, params);
        return 2;
    }

    if (params.language != "auto" && whisper_lang_id(params.language.c_str()) == -1) {
        fprintf(stderr, "error: unknown language '%s'\n", params.language.c_str());
        whisper_print_usage(argc, argv, params);
        exit(0);
    }

    if (params.diarize && params.tinydiarize) {
        fprintf(stderr, "error: cannot use both --diarize and --tinydiarize\n");
        whisper_print_usage(argc, argv, params);
        exit(0);
    }

    if (params.no_prints) {
        whisper_log_set(cb_log_disable, NULL);
    }

    // whisper init

    struct whisper_context_params cparams = whisper_context_default_params();

    cparams.use_gpu = params.use_gpu;
    cparams.flash_attn = params.flash_attn;

    if (!params.dtw.empty()) {
        cparams.dtw_token_timestamps = true;
        cparams.dtw_aheads_preset = WHISPER_AHEADS_NONE;

        if (params.dtw == "tiny")
            cparams.dtw_aheads_preset = WHISPER_AHEADS_TINY;
        if (params.dtw == "tiny.en")
            cparams.dtw_aheads_preset = WHISPER_AHEADS_TINY_EN;
        if (params.dtw == "base")
            cparams.dtw_aheads_preset = WHISPER_AHEADS_BASE;
        if (params.dtw == "base.en")
            cparams.dtw_aheads_preset = WHISPER_AHEADS_BASE_EN;
        if (params.dtw == "small")
            cparams.dtw_aheads_preset = WHISPER_AHEADS_SMALL;
        if (params.dtw == "small.en")
            cparams.dtw_aheads_preset = WHISPER_AHEADS_SMALL_EN;
        if (params.dtw == "medium")
            cparams.dtw_aheads_preset = WHISPER_AHEADS_MEDIUM;
        if (params.dtw == "medium.en")
            cparams.dtw_aheads_preset = WHISPER_AHEADS_MEDIUM_EN;
        if (params.dtw == "large.v1")
            cparams.dtw_aheads_preset = WHISPER_AHEADS_LARGE_V1;
        if (params.dtw == "large.v2")
            cparams.dtw_aheads_preset = WHISPER_AHEADS_LARGE_V2;
        if (params.dtw == "large.v3")
            cparams.dtw_aheads_preset = WHISPER_AHEADS_LARGE_V3;
        if (params.dtw == "large.v3.turbo")
            cparams.dtw_aheads_preset = WHISPER_AHEADS_LARGE_V3_TURBO;

        if (cparams.dtw_aheads_preset == WHISPER_AHEADS_NONE) {
            fprintf(stderr, "error: unknown DTW preset '%s'\n", params.dtw.c_str());
            return 3;
        }
    }

    struct whisper_context *ctx = whisper_init_from_file_with_params(params.model.c_str(), cparams);

    if (ctx == nullptr) {
        fprintf(stderr, "error: failed to initialize whisper context\n");
        return 3;
    }

    // initialize openvino encoder. this has no effect on whisper.cpp builds that don't have OpenVINO configured
    whisper_ctx_init_openvino_encoder(ctx, nullptr, params.openvino_encode_device.c_str(), nullptr);

    if (!params.grammar.empty()) {
        auto &grammar = params.grammar_parsed;
        if (is_file_exist(params.grammar.c_str())) {
            // read grammar from file
            std::ifstream ifs(params.grammar.c_str());
            const std::string txt = std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            grammar = grammar_parser::parse(txt.c_str());
        }
        else {
            // read grammar from string
            grammar = grammar_parser::parse(params.grammar.c_str());
        }

        // will be empty (default) if there are parse errors
        if (grammar.rules.empty()) {
            fprintf(stderr, "error: failed to parse grammar \"%s\"\n", params.grammar.c_str());
            return 4;
        }
        else {
            fprintf(stderr, "%s: grammar:\n", __func__);
            grammar_parser::print_grammar(stderr, grammar);
            fprintf(stderr, "\n");
        }
    }

    for (int f = 0; f < (int)params.fname_inp.size(); ++f) {
        const auto fname_inp = params.fname_inp[f];
        const auto fname_out = f < (int)params.fname_out.size() && !params.fname_out[f].empty() ? params.fname_out[f] : params.fname_inp[f];

        std::vector<float> pcmf32;               // mono-channel F32 PCM
        std::vector<std::vector<float>> pcmf32s; // stereo-channel F32 PCM

        if (!::read_audio_data(fname_inp, pcmf32, pcmf32s, params.diarize)) {
            fprintf(stderr, "error: failed to read audio file '%s'\n", fname_inp.c_str());
            continue;
        }

        if (!whisper_is_multilingual(ctx)) {
            if (params.language != "en" || params.translate) {
                params.language = "en";
                params.translate = false;
                fprintf(stderr, "%s: WARNING: model is not multilingual, ignoring language and translation options\n", __func__);
            }
        }
        if (params.detect_language) {
            params.language = "auto";
        }

        if (!params.no_prints) {
            // print system information
            fprintf(stderr, "\n");
            fprintf(stderr,
                    "system_info: n_threads = %d / %d | %s\n",
                    params.n_threads * params.n_processors,
                    std::thread::hardware_concurrency(),
                    whisper_print_system_info());

            // print some info about the processing
            fprintf(stderr, "\n");
            fprintf(stderr,
                    "%s: processing '%s' (%d samples, %.1f sec), %d threads, %d processors, %d beams + best of %d, lang = %s, task = %s, "
                    "%stimestamps = %d ...\n",
                    __func__,
                    fname_inp.c_str(),
                    int(pcmf32.size()),
                    float(pcmf32.size()) / WHISPER_SAMPLE_RATE,
                    params.n_threads,
                    params.n_processors,
                    params.beam_size,
                    params.best_of,
                    params.language.c_str(),
                    params.translate ? "translate" : "transcribe",
                    params.tinydiarize ? "tdrz = 1, " : "",
                    params.no_timestamps ? 0 : 1);

            fprintf(stderr, "\n");
        }

        // run the inference
        {
            whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

            const bool use_grammar = (!params.grammar_parsed.rules.empty() && !params.grammar_rule.empty());
            wparams.strategy = (params.beam_size > 1 || use_grammar) ? WHISPER_SAMPLING_BEAM_SEARCH : WHISPER_SAMPLING_GREEDY;

            wparams.print_realtime = false;
            wparams.print_progress = params.print_progress;
            wparams.print_timestamps = !params.no_timestamps;
            wparams.print_special = params.print_special;
            wparams.translate = params.translate;
            wparams.language = params.language.c_str();
            wparams.detect_language = params.detect_language;
            wparams.n_threads = params.n_threads;
            wparams.n_max_text_ctx = params.max_context >= 0 ? params.max_context : wparams.n_max_text_ctx;
            wparams.offset_ms = params.offset_t_ms;
            wparams.duration_ms = params.duration_ms;

            wparams.token_timestamps = params.output_wts || params.output_jsn_full || params.max_len > 0;
            wparams.thold_pt = params.word_thold;
            wparams.max_len = params.output_wts && params.max_len == 0 ? 60 : params.max_len;
            wparams.split_on_word = params.split_on_word;
            wparams.audio_ctx = params.audio_ctx;

            wparams.debug_mode = params.debug_mode;

            wparams.tdrz_enable = params.tinydiarize; // [TDRZ]

            wparams.suppress_regex = params.suppress_regex.empty() ? nullptr : params.suppress_regex.c_str();

            wparams.initial_prompt = params.prompt.c_str();

            wparams.greedy.best_of = params.best_of;
            wparams.beam_search.beam_size = params.beam_size;

            wparams.temperature_inc = params.no_fallback ? 0.0f : params.temperature_inc;
            wparams.temperature = params.temperature;

            wparams.entropy_thold = params.entropy_thold;
            wparams.logprob_thold = params.logprob_thold;
            wparams.no_speech_thold = params.no_speech_thold;

            wparams.no_timestamps = params.no_timestamps;

            wparams.suppress_nst = params.suppress_nst;

            whisper_print_user_data user_data = {&params, &pcmf32s, 0};

            const auto &grammar_parsed = params.grammar_parsed;
            auto grammar_rules = grammar_parsed.c_rules();

            if (use_grammar) {
                if (grammar_parsed.symbol_ids.find(params.grammar_rule) == grammar_parsed.symbol_ids.end()) {
                    fprintf(stderr,
                            "%s: warning: grammar rule '%s' not found - skipping grammar sampling\n",
                            __func__,
                            params.grammar_rule.c_str());
                }
                else {
                    wparams.grammar_rules = grammar_rules.data();
                    wparams.n_grammar_rules = grammar_rules.size();
                    wparams.i_start_rule = grammar_parsed.symbol_ids.at(params.grammar_rule);
                    wparams.grammar_penalty = params.grammar_penalty;
                }
            }

            // this callback is called on each new segment
            if (!wparams.print_realtime) {
                wparams.new_segment_callback = whisper_print_segment_callback;
                wparams.new_segment_callback_user_data = &user_data;
            }

            if (wparams.print_progress) {
                wparams.progress_callback = whisper_print_progress_callback;
                wparams.progress_callback_user_data = &user_data;
            }

            // examples for abort mechanism
            // in examples below, we do not abort the processing, but we could if the flag is set to true

            // the callback is called before every encoder run - if it returns false, the processing is aborted
            {
                static bool is_aborted = false; // NOTE: this should be atomic to avoid data race

                wparams.encoder_begin_callback = [](struct whisper_context * /*ctx*/, struct whisper_state * /*state*/, void *user_data) {
                    bool is_aborted = *(bool *)user_data;
                    return !is_aborted;
                };
                wparams.encoder_begin_callback_user_data = &is_aborted;
            }

            // the callback is called before every computation - if it returns true, the computation is aborted
            {
                static bool is_aborted = false; // NOTE: this should be atomic to avoid data race

                wparams.abort_callback = [](void *user_data) {
                    bool is_aborted = *(bool *)user_data;
                    return is_aborted;
                };
                wparams.abort_callback_user_data = &is_aborted;
            }

            if (whisper_full_parallel(ctx, wparams, pcmf32.data(), pcmf32.size(), params.n_processors) != 0) {
                fprintf(stderr, "%s: failed to process audio\n", argv[0]);
                return 10;
            }
        }

        // output stuff
        {
            printf("\n");

            // output to text file
            if (params.output_txt) {
                const auto fname_txt = fname_out + ".txt";
                output_txt(ctx, fname_txt.c_str(), params, pcmf32s);
            }

            // output to VTT file
            if (params.output_vtt) {
                const auto fname_vtt = fname_out + ".vtt";
                output_vtt(ctx, fname_vtt.c_str(), params, pcmf32s);
            }

            // output to SRT file
            if (params.output_srt) {
                const auto fname_srt = fname_out + ".srt";
                output_srt(ctx, fname_srt.c_str(), params, pcmf32s);
            }

            // output to WTS file
            if (params.output_wts) {
                const auto fname_wts = fname_out + ".wts";
                output_wts(ctx, fname_wts.c_str(), fname_inp.c_str(), params, float(pcmf32.size() + 1000) / WHISPER_SAMPLE_RATE, pcmf32s);
            }

            // output to CSV file
            if (params.output_csv) {
                const auto fname_csv = fname_out + ".csv";
                output_csv(ctx, fname_csv.c_str(), params, pcmf32s);
            }

            // output to JSON file
            if (params.output_jsn) {
                const auto fname_jsn = fname_out + ".json";
                output_json(ctx, fname_jsn.c_str(), params, pcmf32s, params.output_jsn_full);
            }

            // output to LRC file
            if (params.output_lrc) {
                const auto fname_lrc = fname_out + ".lrc";
                output_lrc(ctx, fname_lrc.c_str(), params, pcmf32s);
            }

            // output to score file
            if (params.log_score) {
                const auto fname_score = fname_out + ".score.txt";
                output_score(ctx, fname_score.c_str(), params, pcmf32s);
            }
        }
    }

    if (!params.no_prints) {
        whisper_print_timings(ctx);
    }
    whisper_free(ctx);

    return 0;
}
