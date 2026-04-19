#include "make.h"

static char g_exec_line[MAX_LINE];
static char g_exec_show[MAX_LINE];
static char g_exec_run[MAX_LINE];
static u8 g_exec_err;

static int is_valid_cmd_start(unsigned char c) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
        return 1;
    }
    if (c == '.' || c == '_' || c == '/' || c == '\\' || c == ':') {
        return 1;
    }
    return 0;
}

static void sanitize_exec_cmd(char *s) {
    char *r;
    char *w;
    unsigned char c;

    r = s;
    w = s;
    while (*r != '\0') {
        c = (unsigned char)*r;
        if (c == '\t') {
            *w++ = ' ';
        } else if (c >= 32 && c <= 126) {
            *w++ = (char)c;
        }
        r++;
    }
    *w = '\0';
}

int exec_recipe_line(make_ctx_t *ctx, const char *line, const make_opts_t *opts) {
    char *p;
    int silent;
    int ignore_err;
    int rc;

    if (!vars_expand(ctx, line, g_exec_line, sizeof(g_exec_line))) {
        printf("make: expansion overflow in command\n");
        return 1;
    }

    p = g_exec_line;
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    silent = 0;
    ignore_err = 0;
    while (*p == '@' || *p == '-') {
        if (*p == '@') {
            silent = 1;
        } else {
            ignore_err = 1;
        }
        p++;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
    }

    util_trim(p);
    if (*p == '\0') {
        return 0;
    }

    strncpy(g_exec_show, p, sizeof(g_exec_show) - 1);
    g_exec_show[sizeof(g_exec_show) - 1] = '\0';
    strncpy(g_exec_run, g_exec_show, sizeof(g_exec_run) - 1);
    g_exec_run[sizeof(g_exec_run) - 1] = '\0';
    sanitize_exec_cmd(g_exec_show);
    sanitize_exec_cmd(g_exec_run);

    util_trim(g_exec_show);
    util_trim(g_exec_run);
    if (g_exec_run[0] == '\0') {
        return 0;
    }

    MAKE_DIAG("make: cmd len=%d\n", (int)strlen(g_exec_show));

    if (!is_valid_cmd_start((unsigned char)g_exec_run[0])) {
        MAKE_DIAG("make: invalid command start: 0x%02X\n", (unsigned int)(unsigned char)g_exec_run[0]);
        printf("%s\n", g_exec_show);
        if (ignore_err) {
            return 0;
        }
        return 1;
    }

    if (!silent || opts->dry_run) {
        printf("%s\n", g_exec_show);
    }

    ctx->did_work = 1;

    MAKE_LOG("make: recipe exec='%s' silent=%d ignore=%d dry=%d\n", g_exec_show, silent, ignore_err, opts->dry_run);

    if (opts->dry_run) {
        return 0;
    }

    g_exec_err = 0;
    rc = (int)dss_exec_ex(g_exec_run, &g_exec_err);
    MAKE_LOG("make: recipe exit=%d err=%u\n", rc, (unsigned int)g_exec_err);

    if (rc < 0) {
        printf("make: cannot exec (err=%d)\n", (int)g_exec_err);
        printf("%s\n", g_exec_show);
        if (ignore_err) {
            return 0;
        }
        return 1;
    }

    if (rc != 0) {
        printf("make: recipe failed (%d)\n", rc);
        printf("%s\n", g_exec_show);
        if (ignore_err) {
            return 0;
        }
        return rc;
    }

    return 0;
}
