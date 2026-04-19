#include "diff.h"

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
                               unsigned char context) {
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
        if (!util_streq(a->lines[a_take + post], b->lines[b_take + post])) {
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

static void find_resync(line_stream_t *a, line_stream_t *b, int *best_i, int *best_j) {
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
            if (!util_streq(a->lines[i], b->lines[j])) {
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

int diff_compare_files(diff_ctx_t *ctx, const char *left, const char *right, unsigned char emit, unsigned char mode, unsigned char unified_context, int *has_diff, char *err, int err_sz) {
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
        if (!stream_fill(a, 1, err, err_sz, left) || !stream_fill(b, 1, err, err_sz, right)) {
            fclose(fa);
            fclose(fb);
            return 0;
        }

        if (a->count == 0 && b->count == 0) {
            break;
        }

        if (a->count > 0 && b->count > 0 && util_streq(a->lines[0], b->lines[0])) {
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

        if (!stream_fill(a, LOOKAHEAD_LINES, err, err_sz, left) || !stream_fill(b, LOOKAHEAD_LINES, err, err_sz, right)) {
            fclose(fa);
            fclose(fb);
            return 0;
        }

        {
            int a_take;
            int b_take;

            find_resync(a, b, &a_take, &b_take);
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
                                   unified_context);
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
