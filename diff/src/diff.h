#ifndef SPR_DIFF_H
#define SPR_DIFF_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sprinter/dss.h>

#define MAX_CMDLINE 256
#define MAX_ARGV 16

#define MAX_TEXT 256
#define MAX_PATH_TEXT 128

#define MAX_LINE 255
#define LOOKAHEAD_LINES 8
#define MAX_UNIFIED_CONTEXT 16

#define DIFF_MODE_NORMAL 0
#define DIFF_MODE_UNIFIED 1

#ifndef DIFF_VERSION
#define DIFF_VERSION "0.1.00000000"
#endif

#ifndef DIFF_LOG_ENABLE
#define DIFF_LOG_ENABLE 0
#endif

#if DIFF_LOG_ENABLE
#define DIFF_LOG(...) printf(__VA_ARGS__)
#else
#define DIFF_LOG(...) ((void)0)
#endif

typedef struct {
    unsigned char brief;
    unsigned char report_identical;
    unsigned char show_help;
    unsigned char mode;
    unsigned char unified_context;
    char out_path[MAX_PATH_TEXT];
    char left[MAX_PATH_TEXT];
    char right[MAX_PATH_TEXT];
} diff_opts_t;

typedef struct {
    FILE *fp;
    unsigned char eof;
    unsigned char count;
    unsigned int next_line_no;
    unsigned int line_no[LOOKAHEAD_LINES];
    char lines[LOOKAHEAD_LINES][MAX_LINE + 1];
} line_stream_t;

typedef struct {
    FILE *out;
    line_stream_t left_stream;
    line_stream_t right_stream;
    char unified_hist[MAX_UNIFIED_CONTEXT][MAX_LINE + 1];
    unsigned int unified_hist_left_no[MAX_UNIFIED_CONTEXT];
    unsigned int unified_hist_right_no[MAX_UNIFIED_CONTEXT];
} diff_ctx_t;

void ctx_init(diff_ctx_t *ctx);

void util_rtrim(char *s);
void util_ltrim(char *s);
void util_trim(char *s);
int util_streq(const char *a, const char *b);
int util_parse_cmdline(char *buf, char *argv[MAX_ARGV]);
void util_set_error(char *dst, int dst_sz, const char *prefix, const char *detail);

void stream_init(line_stream_t *s, FILE *fp);
int stream_fill(line_stream_t *s, int need, char *err, int err_sz, const char *path);
void stream_consume(line_stream_t *s, int n);
unsigned int stream_empty_anchor(const line_stream_t *s);

int diff_compare_files(diff_ctx_t *ctx, const char *left, const char *right, unsigned char emit, unsigned char mode, unsigned char unified_context, int *has_diff, char *err, int err_sz);

#endif
