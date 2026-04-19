#include "diff.h"

int read_text_lines(diff_ctx_t *ctx, const char *path, const char **out_lines, int *out_count, char *err, int err_sz) {
    FILE *fp;
    char buf[MAX_LINE + 4];
    int count;

    fp = fopen(path, "r");
    if (fp == (FILE *)0) {
        util_set_error(err, err_sz, "cannot open ", path);
        return 0;
    }

    count = 0;
    while (fgets(buf, sizeof(buf), fp) != (char *)0) {
        int len;
        int has_eol;
        const char *saved;

        len = (int)strlen(buf);
        has_eol = 0;
        if (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
            has_eol = 1;
        }

        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
            buf[len - 1] = '\0';
            len--;
        }

        if (!has_eol && len >= MAX_LINE) {
            fclose(fp);
            util_set_error(err, err_sz, "line too long in ", path);
            return 0;
        }

        if (count >= MAX_LINES) {
            fclose(fp);
            util_set_error(err, err_sz, "too many lines in ", path);
            return 0;
        }

        saved = ctx_strdup(ctx, buf);
        if (saved == (const char *)0) {
            fclose(fp);
            util_set_error(err, err_sz, "out of text memory while reading ", path);
            return 0;
        }

        out_lines[count++] = saved;
    }

    fclose(fp);
    *out_count = count;
    return 1;
}
