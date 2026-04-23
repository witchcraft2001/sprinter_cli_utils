#ifndef SPR_DELTREE_H
#define SPR_DELTREE_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <dir.h>
#include <sprinter/dss.h>

#define MAX_CMDLINE 256
#define MAX_ARGV 16
#define MAX_TEXT 256
#define MAX_PATH_TEXT 192
#define MAX_STACK_DEPTH 20
#define MAX_TARGETS 12

#ifndef DELTREE_VERSION
#define DELTREE_VERSION "0.1.00000000"
#endif

#ifndef DELTREE_LOG_ENABLE
#define DELTREE_LOG_ENABLE 0
#endif

#if DELTREE_LOG_ENABLE
#define DELTREE_LOG(...) printf(__VA_ARGS__)
#else
#define DELTREE_LOG(...) ((void)0)
#endif

typedef struct {
    unsigned char show_help;
    unsigned char assume_yes;
    unsigned char path_count;
    char paths[MAX_TARGETS][MAX_PATH_TEXT];
} deltree_opts_t;

void util_rtrim(char *s);
void util_ltrim(char *s);
void util_trim(char *s);
int util_streq(const char *a, const char *b);
int util_strieq(const char *a, const char *b);
int util_parse_cmdline(char *buf, char *argv[MAX_ARGV]);
void util_read_cmdline_safe(char *out, int out_sz);
int util_copy_path(char *dst, int dst_sz, const char *src);
int util_join_path(char *out, int out_sz, const char *base, const char *name);
int util_is_dot_entry(const char *name);
void util_normalize_path(char *path);
int util_has_wildcards(const char *path);
int util_is_root_path(const char *path);

int input_confirm_delete(const char *path, int *aborted);
int input_poll_abort(void);

int fs_probe_path(const char *path, u8 *attr, int *is_dir, u8 *err_code);
int fs_delete_tree(const char *root, char *err, int err_sz);

#endif
