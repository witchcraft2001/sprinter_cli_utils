#ifndef SPR_MAKE_H
#define SPR_MAKE_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <dir.h>
#include <sprinter/dss.h>

#define MAX_TARGETS 64
#define MAX_DEPS 24
#define MAX_CMDS 24
#define MAX_VARS 64
#define MAX_VAR_NAME 32

#define MAX_LINE 256
#define MAX_TEXT 256
#define MAX_CMDLINE 256
#define MAX_ARGV 16
#define TEXT_POOL_SIZE 24576

typedef enum {
    VAR_RECURSIVE = 0,
    VAR_SIMPLE = 1
} var_flavor_t;

typedef struct {
    char name[MAX_VAR_NAME + 1];
    var_flavor_t flavor;
    const char *raw;
    const char *value;
    unsigned char used;
} variable_t;

typedef struct {
    const char *name;
    const char *deps[MAX_DEPS];
    unsigned char dep_count;
    const char *cmds[MAX_CMDS];
    unsigned char cmd_count;
    unsigned char visited;
    unsigned char building;
    unsigned char phony;
} target_t;

typedef struct {
    target_t targets[MAX_TARGETS];
    unsigned char target_count;
    unsigned char first_target_idx;
    variable_t vars[MAX_VARS];
    unsigned char var_count;
    char text_pool[TEXT_POOL_SIZE];
    unsigned int text_pool_pos;
} make_ctx_t;

typedef struct {
    const char *makefile;
    const char *goal;
    unsigned char dry_run;
} make_opts_t;

void util_strip_comment(char *s);
void util_rtrim(char *s);
void util_ltrim(char *s);
void util_trim(char *s);
int util_is_blank(const char *s);
int util_streq(const char *a, const char *b);
int util_strieq(const char *a, const char *b);
int util_parse_cmdline(char *buf, char *argv[MAX_ARGV]);

void ctx_init(make_ctx_t *ctx);
const char *ctx_strdup(make_ctx_t *ctx, const char *s);

int vars_set(make_ctx_t *ctx, const char *name, const char *rhs, var_flavor_t flavor);
int vars_expand(make_ctx_t *ctx, const char *in, char *out, int out_sz);

int parser_load_file(make_ctx_t *ctx, const char *path);
int find_target(make_ctx_t *ctx, const char *name);

int fs_get_mtime(const char *path, unsigned int *date, unsigned int *time);

int exec_recipe_line(make_ctx_t *ctx, const char *line, const make_opts_t *opts);

int build_goal(make_ctx_t *ctx, int idx, const make_opts_t *opts);

#endif
