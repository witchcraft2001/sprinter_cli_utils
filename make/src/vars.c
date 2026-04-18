#include "make.h"

static int var_name_ok(const char *s) {
    if (*s == '\0') {
        return 0;
    }
    while (*s != '\0') {
        if (!(isalnum((unsigned char)*s) || *s == '_')) {
            return 0;
        }
        s++;
    }
    return 1;
}

void ctx_init(make_ctx_t *ctx) {
    ctx->target_count = 0;
    ctx->first_target_idx = 0xFF;
    ctx->var_count = 0;
    ctx->text_pool_pos = 0;
}

const char *ctx_strdup(make_ctx_t *ctx, const char *s) {
    unsigned int n;
    char *p;

    n = (unsigned int)strlen(s) + 1;
    if (ctx->text_pool_pos + n >= TEXT_POOL_SIZE) {
        return (const char *)0;
    }
    p = &ctx->text_pool[ctx->text_pool_pos];
    memcpy(p, s, n);
    ctx->text_pool_pos += n;
    return p;
}

static int find_var(make_ctx_t *ctx, const char *name) {
    int i;

    for (i = 0; i < (int)ctx->var_count; i++) {
        if (ctx->vars[i].used && util_streq(ctx->vars[i].name, name)) {
            return i;
        }
    }
    return -1;
}

static int expand_depth(make_ctx_t *ctx, const char *in, char *out, int out_sz, int depth);

static int var_value(make_ctx_t *ctx, variable_t *v, char *out, int out_sz, int depth) {
    if (v->flavor == VAR_SIMPLE) {
        if (v->value == (const char *)0) {
            out[0] = '\0';
            return 1;
        }
        if ((int)strlen(v->value) >= out_sz) {
            return 0;
        }
        strcpy(out, v->value);
        return 1;
    }
    if (v->raw == (const char *)0) {
        out[0] = '\0';
        return 1;
    }
    return expand_depth(ctx, v->raw, out, out_sz, depth + 1);
}

int vars_set(make_ctx_t *ctx, const char *name, const char *rhs, var_flavor_t flavor) {
    int idx;
    char tmp[MAX_LINE];
    const char *saved;

    if (!var_name_ok(name)) {
        return 0;
    }

    idx = find_var(ctx, name);
    if (idx < 0) {
        if (ctx->var_count >= MAX_VARS) {
            return 0;
        }
        idx = (int)ctx->var_count;
        ctx->var_count++;
        memset(&ctx->vars[idx], 0, sizeof(variable_t));
        strcpy(ctx->vars[idx].name, name);
        ctx->vars[idx].used = 1;
    }

    ctx->vars[idx].flavor = flavor;
    if (flavor == VAR_SIMPLE) {
        if (!vars_expand(ctx, rhs, tmp, sizeof(tmp))) {
            return 0;
        }
        saved = ctx_strdup(ctx, tmp);
        if (saved == (const char *)0) {
            return 0;
        }
        ctx->vars[idx].value = saved;
        ctx->vars[idx].raw = saved;
    } else {
        saved = ctx_strdup(ctx, rhs);
        if (saved == (const char *)0) {
            return 0;
        }
        ctx->vars[idx].raw = saved;
        ctx->vars[idx].value = (const char *)0;
    }

    return 1;
}

static int append_char(char *out, int out_sz, int *pos, char c) {
    if (*pos + 1 >= out_sz) {
        return 0;
    }
    out[*pos] = c;
    (*pos)++;
    out[*pos] = '\0';
    return 1;
}

static int append_text(char *out, int out_sz, int *pos, const char *txt) {
    while (*txt != '\0') {
        if (!append_char(out, out_sz, pos, *txt)) {
            return 0;
        }
        txt++;
    }
    return 1;
}

static int expand_depth(make_ctx_t *ctx, const char *in, char *out, int out_sz, int depth) {
    int pos;
    int i;
    int vi;
    char name[MAX_VAR_NAME + 1];
    char value[MAX_LINE];

    if (depth > 16) {
        return 0;
    }

    pos = 0;
    out[0] = '\0';
    while (*in != '\0') {
        if (in[0] == '$' && in[1] == '(') {
            in += 2;
            i = 0;
            while (*in != '\0' && *in != ')' && i < MAX_VAR_NAME) {
                name[i++] = *in;
                in++;
            }
            name[i] = '\0';
            if (*in == ')') {
                in++;
            }

            vi = find_var(ctx, name);
            if (vi >= 0) {
                if (!var_value(ctx, &ctx->vars[vi], value, sizeof(value), depth)) {
                    return 0;
                }
                if (!append_text(out, out_sz, &pos, value)) {
                    return 0;
                }
            }
        } else {
            if (!append_char(out, out_sz, &pos, *in)) {
                return 0;
            }
            in++;
        }
    }
    return 1;
}

int vars_expand(make_ctx_t *ctx, const char *in, char *out, int out_sz) {
    return expand_depth(ctx, in, out, out_sz, 0);
}
