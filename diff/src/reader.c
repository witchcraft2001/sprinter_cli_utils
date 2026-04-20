#include "diff.h"

static int stream_getc(line_stream_t *s) {
    if (s->has_pushback) {
        s->has_pushback = 0;
        return (int)s->pushback_ch;
    }

    if (s->read_pos >= s->read_len) {
        s->read_len = fread(s->read_buf, 1u, sizeof(s->read_buf), s->fp);
        s->read_pos = 0;
        if (s->read_len == 0) {
            return EOF;
        }
    }

    return (int)(unsigned char)s->read_buf[s->read_pos++];
}

static void stream_ungetc(line_stream_t *s, int ch) {
    if (ch == EOF) {
        return;
    }
    s->has_pushback = 1;
    s->pushback_ch = (unsigned char)ch;
}

static int read_one_line(line_stream_t *s, char *buf, int buf_sz, int *too_long) {
    int ch;
    int len;

    *too_long = 0;

    len = 0;
    while ((ch = stream_getc(s)) != EOF) {
        if (ch == '\r') {
            int next;

            next = stream_getc(s);
            if (next != '\n' && next != EOF) {
                stream_ungetc(s, next);
            }
            break;
        }
        if (ch == '\n') {
            break;
        }
        if (len < buf_sz - 1) {
            buf[len++] = (char)ch;
        } else {
            *too_long = 1;
        }
    }

    if (ch == EOF && len == 0) {
        return 0;
    }

    buf[len] = '\0';

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

        got = read_one_line(s, s->lines[s->count], sizeof(s->lines[s->count]), &too_long);
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
