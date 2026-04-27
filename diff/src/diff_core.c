#include "diff.h"

static int is_hspace(char c) {
    return c == ' ' || c == '\t';
}

static int diff_poll_abort(void) {
    dss_key_t key;
    unsigned char scan;

    if (!dss_kbhit()) {
        return 0;
    }

    dss_waitkey_ex(&key);

    if (key.ascii == 27u) {
        return 1;
    }

    scan = (unsigned char)(key.scan & 0x7Fu);

    if ((key.modifiers & DSS_KEYMOD_CTRL) != 0u) {
        if (key.ascii == 'X' || key.ascii == 'x' ||
            key.ascii == 'Z' || key.ascii == 'z' ||
            key.ascii == 24u || key.ascii == 26u) {
            return 1;
        }

        /* DSS KEYINTER returns Ctrl+letter with ascii=0 and the letter
           encoded only in the position code. Z=0x2A, X=0x2B. */
        if (scan == 0x2Au || scan == 0x2Bu) {
            return 1;
        }
    }

    return 0;
}

static char fold_case_char(char c, unsigned char ignore_case) {
    if (!ignore_case) {
        return c;
    }
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

static int lines_equal_mode(const char *a,
                            const char *b,
                            unsigned char ignore_case,
                            unsigned char ignore_space_change,
                            unsigned char ignore_all_space) {
    if (ignore_all_space) {
        while (*a != '\0' || *b != '\0') {
            while (*a != '\0' && is_hspace(*a)) {
                a++;
            }
            while (*b != '\0' && is_hspace(*b)) {
                b++;
            }
            if (*a == '\0' || *b == '\0') {
                break;
            }
            if (fold_case_char(*a, ignore_case) != fold_case_char(*b, ignore_case)) {
                return 0;
            }
            a++;
            b++;
        }
        while (*a != '\0' && is_hspace(*a)) {
            a++;
        }
        while (*b != '\0' && is_hspace(*b)) {
            b++;
        }
        return (*a == '\0' && *b == '\0');
    }

    if (ignore_space_change) {
        while (*a != '\0' && *b != '\0') {
            if (is_hspace(*a) && is_hspace(*b)) {
                while (*a != '\0' && is_hspace(*a)) {
                    a++;
                }
                while (*b != '\0' && is_hspace(*b)) {
                    b++;
                }
                continue;
            }
            if (is_hspace(*a) || is_hspace(*b)) {
                return 0;
            }
            if (fold_case_char(*a, ignore_case) != fold_case_char(*b, ignore_case)) {
                return 0;
            }
            a++;
            b++;
        }

        while (*a != '\0' && is_hspace(*a)) {
            a++;
        }
        while (*b != '\0' && is_hspace(*b)) {
            b++;
        }
        return (*a == '\0' && *b == '\0');
    }

    while (*a != '\0' && *b != '\0') {
        if (fold_case_char(*a, ignore_case) != fold_case_char(*b, ignore_case)) {
            return 0;
        }
        a++;
        b++;
    }

    return (*a == '\0' && *b == '\0');
}

static void print_range(FILE *out, unsigned int start, unsigned int end, unsigned int empty_anchor) {
    if (start > end) {
        fprintf(out, "%u", empty_anchor);
    } else if (start == end) {
        fprintf(out, "%u", start);
    } else {
        fprintf(out, "%u,%u", start, end);
    }
}

static void print_unified_range(FILE *out, unsigned int start, unsigned int len) {
    fprintf(out, "%u,%u", start, len);
}

static void emit_block_normal(diff_ctx_t *ctx, line_stream_t *a, line_stream_t *b, int a_take, int b_take) {
    FILE *out;
    unsigned int a_start;
    unsigned int a_end;
    unsigned int b_start;
    unsigned int b_end;
    int k;

    out = ctx->out;

    a_start = (a_take > 0) ? a->line_no[0] : stream_empty_anchor(a);
    a_end = (a_take > 0) ? a->line_no[a_take - 1] : a_start;
    b_start = (b_take > 0) ? b->line_no[0] : stream_empty_anchor(b);
    b_end = (b_take > 0) ? b->line_no[b_take - 1] : b_start;

    if (a_take == 0) {
        print_range(out, a_start, a_end, a_end);
        fprintf(out, "a");
        print_range(out, b_start, b_end, b_end);
        fprintf(out, "\n");
        for (k = 0; k < b_take; k++) {
            fprintf(out, "> %s\n", b->lines[k]);
        }
        return;
    }

    if (b_take == 0) {
        print_range(out, a_start, a_end, a_end);
        fprintf(out, "d");
        print_range(out, b_start, b_end, b_end);
        fprintf(out, "\n");
        for (k = 0; k < a_take; k++) {
            fprintf(out, "< %s\n", a->lines[k]);
        }
        return;
    }

    print_range(out, a_start, a_end, a_end);
    fprintf(out, "c");
    print_range(out, b_start, b_end, b_end);
    fprintf(out, "\n");
    for (k = 0; k < a_take; k++) {
        fprintf(out, "< %s\n", a->lines[k]);
    }
    fprintf(out, "---\n");
    for (k = 0; k < b_take; k++) {
        fprintf(out, "> %s\n", b->lines[k]);
    }
}

static void emit_block_unified(diff_ctx_t *ctx,
                               const char hist[MAX_UNIFIED_CONTEXT][MAX_LINE + 1],
                               const unsigned int hist_left_no[MAX_UNIFIED_CONTEXT],
                               const unsigned int hist_right_no[MAX_UNIFIED_CONTEXT],
                               int hist_count,
                               line_stream_t *a,
                               line_stream_t *b,
                               int a_take,
                               int b_take,
                               unsigned char context,
                               unsigned char ignore_case,
                               unsigned char ignore_space_change,
                               unsigned char ignore_all_space) {
    FILE *out;
    int pre;
    int post;
    int k;
    unsigned int old_start;
    unsigned int old_len;
    unsigned int new_start;
    unsigned int new_len;

    out = ctx->out;
    pre = hist_count;
    if (pre > (int)context) {
        pre = (int)context;
    }

    post = 0;
    while (post < (int)context && a_take + post < (int)a->count && b_take + post < (int)b->count) {
        if (!lines_equal_mode(a->lines[a_take + post], b->lines[b_take + post], ignore_case, ignore_space_change, ignore_all_space)) {
            break;
        }
        post++;
    }

    if (pre > 0) {
        old_start = hist_left_no[hist_count - pre];
        new_start = hist_right_no[hist_count - pre];
    } else {
        old_start = (a_take > 0) ? a->line_no[0] : stream_empty_anchor(a) + 1u;
        new_start = (b_take > 0) ? b->line_no[0] : stream_empty_anchor(b) + 1u;
    }

    old_len = (unsigned int)(pre + a_take + post);
    new_len = (unsigned int)(pre + b_take + post);

    fprintf(out, "@@ -");
    print_unified_range(out, old_start, old_len);
    fprintf(out, " +");
    print_unified_range(out, new_start, new_len);
    fprintf(out, " @@\n");

    for (k = hist_count - pre; k < hist_count; k++) {
        fprintf(out, " %s\n", hist[k]);
    }
    for (k = 0; k < a_take; k++) {
        fprintf(out, "-%s\n", a->lines[k]);
    }
    for (k = 0; k < b_take; k++) {
        fprintf(out, "+%s\n", b->lines[k]);
    }
    for (k = 0; k < post; k++) {
        fprintf(out, " %s\n", a->lines[a_take + k]);
    }
}

static void find_resync(line_stream_t *a,
                        line_stream_t *b,
                        unsigned char ignore_case,
                        unsigned char ignore_space_change,
                        unsigned char ignore_all_space,
                        int *best_i,
                        int *best_j) {
    int i;
    int j;
    int found;
    int best_cost;

    found = 0;
    best_cost = 0x7FFF;
    *best_i = 0;
    *best_j = 0;

    for (i = 0; i < (int)a->count; i++) {
        for (j = 0; j < (int)b->count; j++) {
            int cost;

            if (i == 0 && j == 0) {
                continue;
            }
            if (!lines_equal_mode(a->lines[i], b->lines[j], ignore_case, ignore_space_change, ignore_all_space)) {
                continue;
            }

            cost = i + j;
            if (!found || cost < best_cost) {
                found = 1;
                best_cost = cost;
                *best_i = i;
                *best_j = j;
            }
        }
    }

    if (found) {
        return;
    }

    if (a->count > 0 && b->count > 0) {
        *best_i = 1;
        *best_j = 1;
    } else if (a->count > 0) {
        *best_i = 1;
        *best_j = 0;
    } else {
        *best_i = 0;
        *best_j = 1;
    }
}

static void history_push(char hist[MAX_UNIFIED_CONTEXT][MAX_LINE + 1],
                         unsigned int hist_left_no[MAX_UNIFIED_CONTEXT],
                         unsigned int hist_right_no[MAX_UNIFIED_CONTEXT],
                         int *hist_count,
                         const char *line,
                         unsigned int left_no,
                         unsigned int right_no) {
    int i;

    if (*hist_count < MAX_UNIFIED_CONTEXT) {
        strcpy(hist[*hist_count], line);
        hist_left_no[*hist_count] = left_no;
        hist_right_no[*hist_count] = right_no;
        (*hist_count)++;
        return;
    }

    for (i = 1; i < MAX_UNIFIED_CONTEXT; i++) {
        strcpy(hist[i - 1], hist[i]);
        hist_left_no[i - 1] = hist_left_no[i];
        hist_right_no[i - 1] = hist_right_no[i];
    }
    strcpy(hist[MAX_UNIFIED_CONTEXT - 1], line);
    hist_left_no[MAX_UNIFIED_CONTEXT - 1] = left_no;
    hist_right_no[MAX_UNIFIED_CONTEXT - 1] = right_no;
}

int diff_probe_binary(const char *path, int *is_binary, char *err, int err_sz) {
    i16 fd;
    unsigned char buf[128];
    unsigned int total;

    *is_binary = 0;
    fd = dss_open(path, O_RDONLY);
    if (fd < 0) {
        util_set_error(err, err_sz, "cannot open ", path);
        return 0;
    }

    total = 0;
    while (total < 1024u) {
        unsigned int want;
        unsigned int got;
        unsigned int i;

        want = (unsigned int)sizeof(buf);
        if (want > 1024u - total) {
            want = 1024u - total;
        }

        got = (unsigned int)dss_read((u8)fd, buf, want);
        if (got == 0u) {
            break;
        }
        if (got == 0xFFFFu) {
            dss_close((u8)fd);
            util_set_error(err, err_sz, "cannot read ", path);
            return 0;
        }

        for (i = 0; i < got; i++) {
            if (buf[i] == 0u) {
                *is_binary = 1;
                dss_close((u8)fd);
                return 1;
            }
        }
        total += got;
    }

    dss_close((u8)fd);
    return 1;
}

int diff_compare_binary_files(const char *left, const char *right, int *same, char *err, int err_sz) {
    i16 fa;
    i16 fb;
    unsigned char ba[256];
    unsigned char bb[256];

    *same = 0;

    fa = dss_open(left, O_RDONLY);
    if (fa < 0) {
        util_set_error(err, err_sz, "cannot open ", left);
        return 0;
    }

    fb = dss_open(right, O_RDONLY);
    if (fb < 0) {
        dss_close((u8)fa);
        util_set_error(err, err_sz, "cannot open ", right);
        return 0;
    }

    while (1) {
        unsigned int ra;
        unsigned int rb;
        unsigned int i;

        ra = (unsigned int)dss_read((u8)fa, ba, sizeof(ba));
        if (ra == 0xFFFFu) {
            dss_close((u8)fa);
            dss_close((u8)fb);
            util_set_error(err, err_sz, "cannot read ", left);
            return 0;
        }
        rb = (unsigned int)dss_read((u8)fb, bb, sizeof(bb));
        if (rb == 0xFFFFu) {
            dss_close((u8)fa);
            dss_close((u8)fb);
            util_set_error(err, err_sz, "cannot read ", right);
            return 0;
        }

        if (ra != rb) {
            *same = 0;
            dss_close((u8)fa);
            dss_close((u8)fb);
            return 1;
        }

        for (i = 0; i < ra; i++) {
            if (ba[i] != bb[i]) {
                *same = 0;
                dss_close((u8)fa);
                dss_close((u8)fb);
                return 1;
            }
        }

        if (ra == 0u) {
            *same = 1;
            dss_close((u8)fa);
            dss_close((u8)fb);
            return 1;
        }
    }
}

int diff_compare_files(diff_ctx_t *ctx,
                       const char *left,
                       const char *right,
                       unsigned char emit,
                       unsigned char mode,
                       unsigned char unified_context,
                       unsigned char ignore_case,
                       unsigned char ignore_space_change,
                       unsigned char ignore_all_space,
                       int *has_diff,
                       char *err,
                       int err_sz) {
    FILE *fa;
    FILE *fb;
    line_stream_t *a;
    line_stream_t *b;
    int hist_count;
    int header_printed;

    *has_diff = 0;

    fa = fopen(left, "r");
    if (fa == (FILE *)0) {
        util_set_error(err, err_sz, "cannot open ", left);
        return 0;
    }

    fb = fopen(right, "r");
    if (fb == (FILE *)0) {
        fclose(fa);
        util_set_error(err, err_sz, "cannot open ", right);
        return 0;
    }

    a = &ctx->left_stream;
    b = &ctx->right_stream;
    stream_init(a, fa);
    stream_init(b, fb);
    hist_count = 0;
    header_printed = 0;
    while (1) {
        if (diff_poll_abort()) {
            fclose(fa);
            fclose(fb);
            util_set_error(err, err_sz, "", "Aborted");
            return 0;
        }

        if (!stream_fill(a, 1, err, err_sz, left) || !stream_fill(b, 1, err, err_sz, right)) {
            fclose(fa);
            fclose(fb);
            return 0;
        }

        if (a->count == 0 && b->count == 0) {
            break;
        }

        if (a->count > 0 && b->count > 0 && lines_equal_mode(a->lines[0], b->lines[0], ignore_case, ignore_space_change, ignore_all_space)) {
            if (mode == DIFF_MODE_UNIFIED) {
                history_push(ctx->unified_hist,
                             ctx->unified_hist_left_no,
                             ctx->unified_hist_right_no,
                             &hist_count,
                             a->lines[0],
                             a->line_no[0],
                             b->line_no[0]);
            }
            stream_consume(a, 1);
            stream_consume(b, 1);
            continue;
        }

        *has_diff = 1;
        if (!emit) {
            break;
        }

        if (diff_poll_abort()) {
            fclose(fa);
            fclose(fb);
            util_set_error(err, err_sz, "", "Aborted");
            return 0;
        }

        if (!stream_fill(a, LOOKAHEAD_LINES, err, err_sz, left) || !stream_fill(b, LOOKAHEAD_LINES, err, err_sz, right)) {
            fclose(fa);
            fclose(fb);
            return 0;
        }

        {
            int a_take;
            int b_take;

            find_resync(a, b, ignore_case, ignore_space_change, ignore_all_space, &a_take, &b_take);
            if (mode == DIFF_MODE_UNIFIED) {
                if (!header_printed) {
                    fprintf(ctx->out, "--- %s\n", left);
                    fprintf(ctx->out, "+++ %s\n", right);
                    header_printed = 1;
                }
                emit_block_unified(ctx,
                                   ctx->unified_hist,
                                   ctx->unified_hist_left_no,
                                   ctx->unified_hist_right_no,
                                   hist_count,
                                   a,
                                   b,
                                   a_take,
                                   b_take,
                                   unified_context,
                                   ignore_case,
                                   ignore_space_change,
                                   ignore_all_space);
                hist_count = 0;
            } else {
                emit_block_normal(ctx, a, b, a_take, b_take);
            }
            stream_consume(a, a_take);
            stream_consume(b, b_take);
        }
    }

    fclose(fa);
    fclose(fb);
    return 1;
}
