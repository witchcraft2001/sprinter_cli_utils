#include "diff.h"

void ctx_init(diff_ctx_t *ctx) {
    memset(ctx, 0, sizeof(diff_ctx_t));
}

const char *ctx_strdup(diff_ctx_t *ctx, const char *s) {
    unsigned int n;
    char *dst;

    n = (unsigned int)strlen(s) + 1u;
    if (ctx->text_pool_pos + n > TEXT_POOL_SIZE) {
        return (const char *)0;
    }

    dst = &ctx->text_pool[ctx->text_pool_pos];
    memcpy(dst, s, n);
    ctx->text_pool_pos += n;
    return dst;
}

void util_rtrim(char *s) {
    int n;

    n = (int)strlen(s);
    while (n > 0 && (unsigned char)s[n - 1] <= ' ') {
        s[n - 1] = '\0';
        n--;
    }
}

void util_ltrim(char *s) {
    char *p;
    char *d;

    p = s;
    while (*p != '\0' && (unsigned char)*p <= ' ') {
        p++;
    }
    if (p != s) {
        d = s;
        while (*p != '\0') {
            *d++ = *p++;
        }
        *d = '\0';
    }
}

void util_trim(char *s) {
    util_ltrim(s);
    util_rtrim(s);
}

int util_streq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

int util_parse_cmdline(char *buf, char *argv[MAX_ARGV]) {
    int argc;
    char *p;

    argc = 0;
    p = buf;
    while (*p != '\0' && argc < MAX_ARGV) {
        while (*p == ' ') {
            p++;
        }
        if (*p == '\0' || *p == '\r' || *p == '\n') {
            break;
        }
        argv[argc++] = p;
        while (*p != '\0' && *p != ' ' && *p != '\r' && *p != '\n') {
            p++;
        }
        if (*p != '\0') {
            *p = '\0';
            p++;
        }
    }

    return argc;
}

void util_set_error(char *dst, int dst_sz, const char *prefix, const char *detail) {
    int avail;

    if (dst_sz <= 0) {
        return;
    }
    dst[0] = '\0';

    strncpy(dst, prefix, (unsigned int)(dst_sz - 1));
    dst[dst_sz - 1] = '\0';

    avail = dst_sz - 1 - (int)strlen(dst);
    if (avail <= 0) {
        return;
    }

    strncat(dst, detail, (unsigned int)avail);
}
