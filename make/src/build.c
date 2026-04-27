#include "make.h"

static int newer(unsigned int d1, unsigned int t1, unsigned int d2, unsigned int t2) {
    if (d1 > d2) {
        return 1;
    }
    if (d1 == d2 && t1 > t2) {
        return 1;
    }
    return 0;
}

int build_goal(make_ctx_t *ctx, int idx, const make_opts_t *opts) {
    target_t *t;
    int i;
    int cmdn;
    int dep_idx;
    int rc;
    unsigned int t_date;
    unsigned int t_time;
    unsigned int d_date;
    unsigned int d_time;
    int t_exists;
    int d_exists;
    int need_run;
    const char *cmds[MAX_CMDS];

    if (idx < 0 || idx >= (int)ctx->target_count) {
        return 1;
    }

    t = &ctx->targets[idx];
    MAKE_LOG("make: enter target '%s'\n", t->name);
    if (t->visited) {
        MAKE_LOG("make: target already visited '%s'\n", t->name);
        return 0;
    }
    if (t->building) {
        printf("make: dependency cycle at target '%s'\n", t->name);
        return 1;
    }

    t->building = 1;
    t_exists = fs_get_mtime(t->name, &t_date, &t_time);
    need_run = (t->phony || !t_exists);
    MAKE_LOG("make: target '%s' exists=%d phony=%d need=%d\n", t->name, t_exists, t->phony, need_run);

    for (i = 0; i < (int)t->dep_count; i++) {
        dep_idx = find_target(ctx, t->deps[i]);
        if (dep_idx >= 0) {
            rc = build_goal(ctx, dep_idx, opts);
            if (rc != 0) {
                t->building = 0;
                return rc;
            }
            if (ctx->targets[dep_idx].phony) {
                need_run = 1;
                MAKE_LOG("make: dep '%s' is phony -> rebuild '%s'\n", t->deps[i], t->name);
            }
        }

        d_exists = fs_get_mtime(t->deps[i], &d_date, &d_time);
        if (!d_exists && dep_idx < 0) {
            printf("make: missing dependency '%s' for '%s'\n", t->deps[i], t->name);
            t->building = 0;
            return 1;
        }

        if (d_exists && t_exists && newer(d_date, d_time, t_date, t_time)) {
            need_run = 1;
            MAKE_LOG("make: dep '%s' newer than '%s'\n", t->deps[i], t->name);
        }
    }

    if (need_run) {
        MAKE_LOG("make: run recipes for '%s'\n", t->name);
        if (t->cmd_count == 0) {
            if (!t->phony && !fs_get_mtime(t->name, &t_date, &t_time)) {
                printf("make: no recipe to build target '%s'\n", t->name);
                t->building = 0;
                return 1;
            }
        }

        cmdn = (int)t->cmd_count;
        if (cmdn > MAX_CMDS) {
            cmdn = MAX_CMDS;
        }
        for (i = 0; i < cmdn; i++) {
            cmds[i] = t->cmds[i];
        }

        for (i = 0; i < cmdn; i++) {
            rc = exec_recipe_line(ctx, cmds[i], opts);
            if (rc != 0) {
                t->building = 0;
                return rc;
            }
        }

        if (!t->phony && !opts->dry_run && !fs_get_mtime(t->name, &t_date, &t_time)) {
            printf("make: recipe did not create target '%s'\n", t->name);
            t->building = 0;
            return 1;
        }
    }

    MAKE_LOG("make: done target '%s'\n", t->name);

    t->visited = 1;
    t->building = 0;
    return 0;
}
