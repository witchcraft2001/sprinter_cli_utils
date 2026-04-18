#include "make.h"

static int split_tokens(char *s, const char *arr[MAX_DEPS], int max_tokens) {
    int n;
    char *p;

    n = 0;
    p = s;
    while (*p != '\0' && n < max_tokens) {
        while (*p != '\0' && (unsigned char)*p <= ' ') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        arr[n++] = p;
        while (*p != '\0' && (unsigned char)*p > ' ') {
            p++;
        }
        if (*p != '\0') {
            *p = '\0';
            p++;
        }
    }
    return n;
}

int find_target(make_ctx_t *ctx, const char *name) {
    int i;
    for (i = 0; i < (int)ctx->target_count; i++) {
        if (util_strieq(ctx->targets[i].name, name)) {
            return i;
        }
    }
    return -1;
}

static int ensure_target(make_ctx_t *ctx, const char *name) {
    int idx;
    const char *saved;

    idx = find_target(ctx, name);
    if (idx >= 0) {
        return idx;
    }

    if (ctx->target_count >= MAX_TARGETS) {
        return -1;
    }

    saved = ctx_strdup(ctx, name);
    if (saved == (const char *)0) {
        return -1;
    }

    idx = (int)ctx->target_count;
    memset(&ctx->targets[idx], 0, sizeof(target_t));
    ctx->targets[idx].name = saved;
    ctx->target_count++;

    if (ctx->first_target_idx == 0xFF && !util_streq(name, ".PHONY")) {
        ctx->first_target_idx = (unsigned char)idx;
    }

    return idx;
}

static int parse_assignment_line(make_ctx_t *ctx, char *line) {
    char *op;
    char name[MAX_VAR_NAME + 1];
    char rhs[MAX_LINE];
    int n;
    var_flavor_t flavor;

    op = line;
    while (*op != '\0') {
        if (op[0] == ':' && op[1] == '=') {
            break;
        }
        op++;
    }

    if (*op != '\0') {
        flavor = VAR_SIMPLE;
    } else {
        op = strchr(line, '=');
        if (op == (char *)0) {
            return 0;
        }
        flavor = VAR_RECURSIVE;
    }

    n = (int)(op - line);
    while (n > 0 && (unsigned char)line[n - 1] <= ' ') {
        n--;
    }
    if (n <= 0 || n > MAX_VAR_NAME) {
        return 0;
    }
    memcpy(name, line, (unsigned int)n);
    name[n] = '\0';

    if (flavor == VAR_SIMPLE) {
        op += 2;
    } else {
        op += 1;
    }
    while (*op != '\0' && (unsigned char)*op <= ' ') {
        op++;
    }
    strcpy(rhs, op);

    util_trim(name);
    return vars_set(ctx, name, rhs, flavor);
}

static int add_dependency_list(make_ctx_t *ctx, int t_idx, const char *dep_text) {
    char tmp[MAX_LINE];
    char expanded[MAX_LINE];
    const char *tok[MAX_DEPS];
    int n;
    int i;
    const char *saved;

    if (!vars_expand(ctx, dep_text, expanded, sizeof(expanded))) {
        return 0;
    }
    strcpy(tmp, expanded);

    n = split_tokens(tmp, tok, MAX_DEPS);
    for (i = 0; i < n; i++) {
        if (ctx->targets[t_idx].dep_count >= MAX_DEPS) {
            return 0;
        }
        saved = ctx_strdup(ctx, tok[i]);
        if (saved == (const char *)0) {
            return 0;
        }
        ctx->targets[t_idx].deps[ctx->targets[t_idx].dep_count++] = saved;
    }
    return 1;
}

static int mark_phony_targets(make_ctx_t *ctx, const char *dep_text) {
    char tmp[MAX_LINE];
    char expanded[MAX_LINE];
    const char *tok[MAX_DEPS];
    int n;
    int i;
    int idx;

    if (!vars_expand(ctx, dep_text, expanded, sizeof(expanded))) {
        return 0;
    }
    strcpy(tmp, expanded);

    n = split_tokens(tmp, tok, MAX_DEPS);
    for (i = 0; i < n; i++) {
        idx = ensure_target(ctx, tok[i]);
        if (idx < 0) {
            return 0;
        }
        ctx->targets[idx].phony = 1;
    }
    return 1;
}

static int parse_rule_line(make_ctx_t *ctx, char *line, int *current_target) {
    char *colon;
    char lhs[MAX_LINE];
    char rhs[MAX_LINE];
    char target_name[MAX_LINE];
    int lhs_len;
    int idx;

    colon = strchr(line, ':');
    if (colon == (char *)0) {
        return 0;
    }

    lhs_len = (int)(colon - line);
    if (lhs_len <= 0 || lhs_len >= MAX_LINE) {
        return 0;
    }

    memcpy(lhs, line, (unsigned int)lhs_len);
    lhs[lhs_len] = '\0';
    strcpy(rhs, colon + 1);
    util_trim(lhs);
    util_trim(rhs);

    if (!vars_expand(ctx, lhs, target_name, sizeof(target_name))) {
        return 0;
    }
    util_trim(target_name);

    if (util_streq(target_name, ".PHONY")) {
        *current_target = -1;
        return mark_phony_targets(ctx, rhs);
    }

    idx = ensure_target(ctx, target_name);
    if (idx < 0) {
        return 0;
    }

    ctx->targets[idx].dep_count = 0;
    ctx->targets[idx].cmd_count = 0;
    *current_target = idx;

    return add_dependency_list(ctx, idx, rhs);
}

static int add_recipe_line(make_ctx_t *ctx, int current_target, char *line) {
    char expanded[MAX_LINE];
    const char *saved;

    if (current_target < 0) {
        return 0;
    }
    if (ctx->targets[current_target].cmd_count >= MAX_CMDS) {
        return 0;
    }

    if (!vars_expand(ctx, line, expanded, sizeof(expanded))) {
        return 0;
    }
    saved = ctx_strdup(ctx, expanded);
    if (saved == (const char *)0) {
        return 0;
    }

    ctx->targets[current_target].cmds[ctx->targets[current_target].cmd_count++] = saved;
    return 1;
}

int parser_load_file(make_ctx_t *ctx, const char *path) {
    FILE *fp;
    char line[MAX_LINE];
    int current_target;

    fp = fopen(path, "r");
    if (fp == (FILE *)0) {
        MAKE_LOG("make: cannot open '%s'\n", path);
        return 0;
    }

    MAKE_LOG("make: parsing '%s'\n", path);

    current_target = -1;
    while (fgets(line, sizeof(line), fp) != (char *)0) {
        util_rtrim(line);
        util_strip_comment(line);
        util_rtrim(line);

        if (util_is_blank(line)) {
            continue;
        }

        if (line[0] == ' ' || line[0] == '\t') {
            util_ltrim(line);
            if (!add_recipe_line(ctx, current_target, line)) {
                MAKE_LOG("make: recipe parse error\n");
                fclose(fp);
                return 0;
            }
            continue;
        }

        if (parse_assignment_line(ctx, line)) {
            MAKE_LOG("make: variable parsed\n");
            current_target = -1;
            continue;
        }

        if (!parse_rule_line(ctx, line, &current_target)) {
            MAKE_LOG("make: rule parse error: %s\n", line);
            fclose(fp);
            return 0;
        }
        MAKE_LOG("make: rule parsed\n");
    }

    fclose(fp);
    return 1;
}
