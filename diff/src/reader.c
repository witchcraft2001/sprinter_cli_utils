#include "diff.h"

static int read_one_line(FILE *fp, char *buf, int buf_sz, int *too_long) {
    int len;

    *too_long = 0;
    if (fgets(buf, buf_sz, fp) == (char *)0) {
        return 0;
    }

    len = (int)strlen(buf);
    if (len > 0 && buf[len - 1] != '\n' && buf[len - 1] != '\r') {
        if (!feof(fp)) {
            int ch;
            *too_long = 1;
            while ((ch = fgetc(fp)) != EOF) {
                if (ch == '\n') {
                    break;
                }
            }
        }
    }

    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
        buf[len - 1] = '\0';
        len--;
    }

    return 1;
}

void stream_init(line_stream_t *s, FILE *fp) {
    memset(s, 0, sizeof(line_stream_t));
    s->fp = fp;
    s->next_line_no = 1;
}

int stream_fill(line_stream_t *s, int need, char *err, int err_sz, const char *path) {
    while ((int)s->count < need && !s->eof) {
        int too_long;
        int got;

        got = read_one_line(s->fp, s->lines[s->count], sizeof(s->lines[s->count]), &too_long);
        if (!got) {
            s->eof = 1;
            break;
        }
        if (too_long) {
            util_set_error(err, err_sz, "line too long in ", path);
            return 0;
        }

        s->line_no[s->count] = s->next_line_no;
        s->next_line_no++;
        s->count++;
    }

    return 1;
}

void stream_consume(line_stream_t *s, int n) {
    int i;
    int remain;

    if (n <= 0) {
        return;
    }
    if (n >= (int)s->count) {
        s->count = 0;
        return;
    }

    remain = (int)s->count - n;
    for (i = 0; i < remain; i++) {
        s->line_no[i] = s->line_no[i + n];
        strcpy(s->lines[i], s->lines[i + n]);
    }
    s->count = (unsigned char)remain;
}

unsigned int stream_empty_anchor(const line_stream_t *s) {
    if (s->count > 0) {
        return (unsigned int)(s->line_no[0] - 1u);
    }
    if (s->next_line_no == 0) {
        return 0;
    }
    return (unsigned int)(s->next_line_no - 1u);
}
