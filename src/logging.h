#pragma once

#include <cstdio>
#include <cstdint>

#define log_at_level(level, append_nl, ...) lprint(GLOBAL_LOGGER, level, __FILE__, __func__, __LINE__, append_nl, __VA_ARGS__)
#define tlog(...) log_at_level(LOG_TRACE, true, __VA_ARGS__)
#define dlog(...) log_at_level(LOG_DEBUG, true, __VA_ARGS__)
#define ilog(...) log_at_level(LOG_INFO, true, __VA_ARGS__)
#define wlog(...) log_at_level(LOG_WARN, true, __VA_ARGS__)
#define elog(...) log_at_level(LOG_ERROR, true, __VA_ARGS__)
#define flog(...) log_at_level(LOG_FATAL, true, __VA_ARGS__)

struct tm;
struct logging_ctxt;

extern logging_ctxt *GLOBAL_LOGGER;

struct log_event
{
    va_list ap;
    const char *fmt;
    const char *file;
    const char *func;
    tm *time;
    void *udata;
    int line;
    int level;
    uint64_t thread_id;
    bool append_nl;
};

using logging_cbfn = void(log_event *ev);
using logging_lock_cbfn = void(bool lock, void *udata);

struct logging_cb_data
{
    logging_cbfn *fn;
    void *udata;
    int level;
};

struct lock_cb_data
{
    logging_lock_cbfn *fn;
    void *udata;
};

enum
{
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
};

logging_ctxt *create_logger(const char *name, int level, bool quiet);
void destroy_logger(logging_ctxt *logger);
const char *logging_level_string(logging_ctxt *logger, int level);
void set_logging_lock(logging_ctxt *logger, const lock_cb_data &cb_data);
void set_logging_level(logging_ctxt *logger, int level);
int logging_level(logging_ctxt *logger);
void set_quiet_logging(logging_ctxt *logger, bool enable);
int add_logging_fp(logging_ctxt *logger, FILE *fp, int level);
int add_logging_callback(logging_ctxt *logger, const logging_cb_data &cb_data);
void lprint(logging_ctxt *logger, int level, const char *file, const char *func, int line, bool append_newline, const char *fmt, ...);

