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

#define MAX_LINES 64
#define MAX_LINE 160
#define TEXT_POOL_SIZE 4096

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
    char left[MAX_PATH_TEXT];
    char right[MAX_PATH_TEXT];
} diff_opts_t;

typedef struct {
    const char *a[MAX_LINES];
    const char *b[MAX_LINES];
    int a_count;
    int b_count;
    unsigned char dir[MAX_LINES + 1][MAX_LINES + 1];
    unsigned char lcs_len;
    char text_pool[TEXT_POOL_SIZE];
    unsigned int text_pool_pos;
} diff_ctx_t;

void ctx_init(diff_ctx_t *ctx);
const char *ctx_strdup(diff_ctx_t *ctx, const char *s);

void util_rtrim(char *s);
void util_ltrim(char *s);
void util_trim(char *s);
int util_streq(const char *a, const char *b);
int util_parse_cmdline(char *buf, char *argv[MAX_ARGV]);
void util_set_error(char *dst, int dst_sz, const char *prefix, const char *detail);

int read_text_lines(diff_ctx_t *ctx, const char *path, const char **out_lines, int *out_count, char *err, int err_sz);

void diff_build_lcs(diff_ctx_t *ctx);
int diff_has_changes(const diff_ctx_t *ctx);
void diff_emit_normal(const diff_ctx_t *ctx);

#endif
