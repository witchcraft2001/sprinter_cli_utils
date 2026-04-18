#include "make.h"

int exec_recipe_line(make_ctx_t *ctx, const char *line, const make_opts_t *opts) {
    char expanded[MAX_LINE];
    char cmd[MAX_LINE];
    char *p;
    int silent;
    int ignore_err;
    int rc;

    if (!vars_expand(ctx, line, expanded, sizeof(expanded))) {
        printf("make: expansion overflow in command\n");
        return 1;
    }

    strcpy(cmd, expanded);
    p = cmd;
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    silent = 0;
    ignore_err = 0;
    while (*p == '@' || *p == '-') {
        if (*p == '@') {
            silent = 1;
        } else if (*p == '-') {
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

    if (!silent || opts->dry_run) {
        printf("%s\n", p);
    }

    if (opts->dry_run) {
        return 0;
    }

    rc = (int)dss_exec(p);
    if (rc < 0) {
        printf("make: cannot exec: %s\n", p);
        if (ignore_err) {
            return 0;
        }
        return 1;
    }
    if (rc != 0) {
        printf("make: recipe failed (%d): %s\n", rc, p);
        if (ignore_err) {
            return 0;
        }
        return rc;
    }

    return 0;
}
