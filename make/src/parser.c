#include "make.h"

static char g_assign_name[MAX_VAR_NAME + 1];
static char g_assign_rhs[MAX_LINE];
static char g_deps_tmp[MAX_LINE];
static char g_deps_expanded[MAX_LINE];
static char g_phony_tmp[MAX_LINE];
static char g_phony_expanded[MAX_LINE];
static char g_rule_lhs[MAX_LINE];
static char g_rule_rhs[MAX_LINE];
static char g_rule_target_name[MAX_LINE];
static char g_recipe_expanded[MAX_LINE];
static char g_recipe_cleaned[MAX_LINE];
static char g_parse_line[MAX_LINE];
static int g_current_target;

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
    memcpy(g_assign_name, line, (unsigned int)n);
    g_assign_name[n] = '\0';

    if (flavor == VAR_SIMPLE) {
        op += 2;
    } else {
        op += 1;
    }
    while (*op != '\0' && (unsigned char)*op <= ' ') {
        op++;
    }
    strcpy(g_assign_rhs, op);

    util_trim(g_assign_name);
    return vars_set(ctx, g_assign_name, g_assign_rhs, flavor);
}

static int add_dependency_list(make_ctx_t *ctx, int t_idx, const char *dep_text) {
    const char *tok[MAX_DEPS];
    int n;
    int i;
    const char *saved;

    if (!vars_expand(ctx, dep_text, g_deps_expanded, sizeof(g_deps_expanded))) {
        return 0;
    }
    strcpy(g_deps_tmp, g_deps_expanded);

    n = split_tokens(g_deps_tmp, tok, MAX_DEPS);
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
    const char *tok[MAX_DEPS];
    int n;
    int i;
    int idx;

    if (!vars_expand(ctx, dep_text, g_phony_expanded, sizeof(g_phony_expanded))) {
        return 0;
    }
    strcpy(g_phony_tmp, g_phony_expanded);

    n = split_tokens(g_phony_tmp, tok, MAX_DEPS);
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
    int lhs_len;
    int idx;
    int ok;

    colon = strchr(line, ':');
    if (colon == (char *)0) {
        return 0;
    }

    lhs_len = (int)(colon - line);
    if (lhs_len <= 0 || lhs_len >= MAX_LINE) {
        return 0;
    }

    memcpy(g_rule_lhs, line, (unsigned int)lhs_len);
    g_rule_lhs[lhs_len] = '\0';
    strcpy(g_rule_rhs, colon + 1);
    util_trim(g_rule_lhs);
    util_trim(g_rule_rhs);

    if (!vars_expand(ctx, g_rule_lhs, g_rule_target_name, sizeof(g_rule_target_name))) {
        return 0;
    }
    util_trim(g_rule_target_name);

    if (util_streq(g_rule_target_name, ".PHONY")) {
        *current_target = -1;
        return mark_phony_targets(ctx, g_rule_rhs);
    }

    idx = ensure_target(ctx, g_rule_target_name);
    if (idx < 0) {
        return 0;
    }

    ctx->targets[idx].dep_count = 0;
    ctx->targets[idx].cmd_count = 0;
    *current_target = idx;

    MAKE_DIAG("diag: rule target '%s' idx=%d before deps current=%d\n",
              g_rule_target_name, idx, *current_target);
    ok = add_dependency_list(ctx, idx, g_rule_rhs);
    MAKE_DIAG("diag: rule target '%s' idx=%d after deps current=%d ok=%d deps=%d\n",
              g_rule_target_name, idx, *current_target, ok, (int)ctx->targets[idx].dep_count);
    return ok;
}

static int add_recipe_line(make_ctx_t *ctx, int current_target, char *line) {
    int i;
    int j;
    unsigned char c;
    const char *saved;

    if (current_target < 0) {
        MAKE_DIAG("diag: recipe has no target\n");
        return 0;
    }
    if (ctx->targets[current_target].cmd_count >= MAX_CMDS) {
        MAKE_DIAG("diag: recipe cmd limit target=%d\n", current_target);
        return 0;
    }

    if (!vars_expand(ctx, line, g_recipe_expanded, sizeof(g_recipe_expanded))) {
        MAKE_DIAG("diag: recipe expand failed: %s\n", line);
        return 0;
    }

    i = 0;
    j = 0;
    while (g_recipe_expanded[i] != '\0' && j < (int)sizeof(g_recipe_cleaned) - 1) {
        c = (unsigned char)g_recipe_expanded[i++];
        if (c == '\t') {
            g_recipe_cleaned[j++] = ' ';
        } else if (c >= 32 && c <= 126) {
            g_recipe_cleaned[j++] = (char)c;
        }
    }
    g_recipe_cleaned[j] = '\0';

    saved = ctx_strdup(ctx, g_recipe_cleaned);
    if (saved == (const char *)0) {
        MAKE_DIAG("diag: recipe text pool full\n");
        return 0;
    }

    ctx->targets[current_target].cmds[ctx->targets[current_target].cmd_count++] = saved;
    return 1;
}

#if MAKE_DIAG_ENABLE
static void diag_stdio_slots(void) {
    int i;

    for (i = 0; i < FOPEN_MAX; i++) {
        printf("diag: stdio[%d] fd=%d fl=%d un=%d\n",
               i,
               (int)_stdio_files[i].fd,
               (int)_stdio_files[i].flags,
               (int)_stdio_files[i].ungetc_buf);
    }
}

static void diag_dss_open_probe(const char *path) {
    i16 fd;

    fd = dss_open(path, O_RDONLY);
    if (fd >= 0) {
        printf("diag: dss_open '%s' ok fd=%d\n", path, (int)fd);
        dss_close((u8)fd);
    } else {
        printf("diag: dss_open '%s' failed\n", path);
    }
}
#endif

int parser_load_file(make_ctx_t *ctx, const char *path) {
    FILE *fp;
    int line_no;

    fp = fopen(path, "r");
    if (fp == (FILE *)0) {
        MAKE_DIAG("diag: fopen '%s' failed\n", path);
#if MAKE_DIAG_ENABLE
        diag_dss_open_probe(path);
        diag_stdio_slots();
#endif
        MAKE_LOG("make: cannot open '%s'\n", path);
        return -1;
    }

    MAKE_DIAG("diag: fopen '%s' ok fp=%u fd=%d fl=%d\n",
              path,
              (unsigned int)fp,
              (int)fp->fd,
              (int)fp->flags);
    MAKE_LOG("make: parsing '%s'\n", path);

    g_current_target = -1;
    line_no = 0;
    while (fgets(g_parse_line, sizeof(g_parse_line), fp) != (char *)0) {
        line_no++;
        util_rtrim(g_parse_line);
        util_strip_comment(g_parse_line);
        util_rtrim(g_parse_line);

        if (util_is_blank(g_parse_line)) {
            continue;
        }

        if (g_parse_line[0] == ' ' || g_parse_line[0] == '\t') {
            util_ltrim(g_parse_line);
            MAKE_DIAG("diag: recipe line %d target=%d text='%s'\n", line_no, g_current_target, g_parse_line);
            if (!add_recipe_line(ctx, g_current_target, g_parse_line)) {
                MAKE_DIAG("diag: recipe parse error line %d\n", line_no);
                MAKE_LOG("make: recipe parse error\n");
                fclose(fp);
                return 0;
            }
            continue;
        }

        if (parse_assignment_line(ctx, g_parse_line)) {
            MAKE_LOG("make: variable parsed\n");
            MAKE_DIAG("diag: assignment line %d vars=%d\n", line_no, (int)ctx->var_count);
            g_current_target = -1;
            continue;
        }

        if (!parse_rule_line(ctx, g_parse_line, &g_current_target)) {
            MAKE_DIAG("diag: rule parse error line %d text='%s'\n", line_no, g_parse_line);
            MAKE_LOG("make: rule parse error: %s\n", g_parse_line);
            fclose(fp);
            return 0;
        }
        MAKE_LOG("make: rule parsed\n");
        MAKE_DIAG("diag: rule line %d target=%d targets=%d\n", line_no, g_current_target, (int)ctx->target_count);
    }

    fclose(fp);
    MAKE_DIAG("diag: parsed '%s' lines=%d targets=%d\n", path, line_no, (int)ctx->target_count);
    return 1;
}
