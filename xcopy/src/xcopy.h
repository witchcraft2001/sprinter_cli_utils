#ifndef SPR_XCOPY_H
#define SPR_XCOPY_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <sprinter/bios.h>
#include <sprinter/dss.h>
#include <sprinter/ports.h>

#define MAX_CMDLINE 256
#define MAX_ARGV 16
#define MAX_TEXT 256
#define MAX_PATH_TEXT 192
/* Directory traversal is iterative; keep pending directory queue bounded. */
#define MAX_STACK_DEPTH 8
#define XCOPY_MAX_DIR_ENTRIES 64
#define XCOPY_ENTRY_NAME_MAX 32

#define XCOPY_PAGE_SIZE 16384u
#define XCOPY_MAX_BUFFER_PAGES 4

#define XCOPY_CONFIRM_NO 0
#define XCOPY_CONFIRM_YES 1
#define XCOPY_CONFIRM_ALL 2

#define XCOPY_DEST_FILE 1
#define XCOPY_DEST_DIR 2

#ifndef XCOPY_VERSION
#define XCOPY_VERSION "0.1.00000000"
#endif

#ifndef XCOPY_LOG_ENABLE
#define XCOPY_LOG_ENABLE 0
#endif

#if XCOPY_LOG_ENABLE
#define XCOPY_LOG(...) printf(__VA_ARGS__)
#else
#define XCOPY_LOG(...) ((void)0)
#endif

typedef struct {
    unsigned char show_help;
    unsigned char assume_yes;
    unsigned char force_prompt;
    unsigned char copy_empty;
    unsigned char include_hidden;
    unsigned char preserve_attr;
    unsigned char verbose;
    unsigned char dst_hint_dir;
    char src[MAX_PATH_TEXT];
    char dst[MAX_PATH_TEXT];
} xcopy_opts_t;

typedef struct {
    u8 pages[XCOPY_MAX_BUFFER_PAGES];
    u8 page_count;
    u8 saved_win3;
    unsigned char has_saved_win3;
    u16 used[XCOPY_MAX_BUFFER_PAGES];
} xcopy_buffer_t;

typedef struct {
    xcopy_opts_t opts;
    xcopy_buffer_t buffer;
    unsigned char overwrite_all;
    unsigned char aborted;
    unsigned char had_error;
    u16 total_pages;
    u16 free_pages_before;
    u32 files_copied;
    u32 files_skipped;
    u32 dirs_created;
    u32 bytes_copied;
    u32 start_cs;
    u32 end_cs;
} xcopy_ctx_t;

void util_rtrim(char *s);
void util_ltrim(char *s);
void util_trim(char *s);
int util_streq(const char *a, const char *b);
int util_strieq(const char *a, const char *b);
int util_parse_cmdline(char *buf, char *argv[MAX_ARGV]);
void util_read_cmdline_safe(char *out, int out_sz);
int util_copy_path(char *dst, int dst_sz, const char *src);
int util_join_path(char *out, int out_sz, const char *base, const char *name);
void util_normalize_path(char *path);
int util_is_dot_entry(const char *name);
int util_has_wildcards(const char *path);
int util_has_trailing_sep(const char *path);
int util_get_basename(const char *path, char *name, int name_sz);
int util_parent_path(const char *path, char *parent, int parent_sz);
int util_split_parent_mask(const char *path, char *parent, int parent_sz, char *mask, int mask_sz);
void util_set_error(char *dst, int dst_sz, const char *prefix, const char *detail);
u32 util_now_centis(void);
u32 util_mul_u32_u8(u32 value, u8 factor);
int util_path_is_same(const char *a, const char *b);
int util_path_is_subpath(const char *path, const char *root);

int input_poll_abort(void);
int input_confirm_overwrite(const char *path, int *aborted);
int input_confirm_dest_kind(const char *path, int *aborted);

int buffer_init(xcopy_buffer_t *buf, u16 *total_pages, u16 *free_pages_before);
void buffer_free(xcopy_buffer_t *buf);
u32 buffer_capacity_bytes(const xcopy_buffer_t *buf);
int buffer_load_pages(xcopy_buffer_t *buf,
                      xcopy_ctx_t *ctx,
                      u8 fd,
                      u32 *loaded_total,
                      int *hit_eof,
                      char *err,
                      int err_sz);
int buffer_flush_pages(xcopy_buffer_t *buf,
                       xcopy_ctx_t *ctx,
                       u8 fd,
                       char *err,
                       int err_sz);

int fs_probe_path(const char *path, u8 *attr, int *is_dir);
int fs_getattr(const char *path, u8 *attr, u8 *err_code);
int fs_setattr(const char *path, u8 attr, u8 *err_code);
int fs_ensure_dir_tree(const char *path, u16 *created_count, char *err, int err_sz);
int fs_ensure_parent_dir(const char *path, u16 *created_count, char *err, int err_sz);
int xcopy_run(xcopy_ctx_t *ctx, char *err, int err_sz);
void xcopy_print_stats(const xcopy_ctx_t *ctx);

#endif
