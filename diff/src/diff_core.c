#include "diff.h"

static int lines_equal(const char *a, const char *b) {
    return util_streq(a, b);
}

void diff_build_lcs(diff_ctx_t *ctx) {
    int i;
    int j;
    int n;
    int m;
    unsigned char prev[MAX_LINES + 1];
    unsigned char curr[MAX_LINES + 1];

    n = ctx->a_count;
    m = ctx->b_count;

    memset(prev, 0, sizeof(prev));
    memset(curr, 0, sizeof(curr));

    for (i = n - 1; i >= 0; i--) {
        curr[m] = 0;
        for (j = m - 1; j >= 0; j--) {
            if (lines_equal(ctx->a[i], ctx->b[j])) {
                curr[j] = (unsigned char)(prev[j + 1] + 1);
                ctx->dir[i][j] = (unsigned char)'M';
            } else if (prev[j] >= curr[j + 1]) {
                curr[j] = prev[j];
                ctx->dir[i][j] = (unsigned char)'U';
            } else {
                curr[j] = curr[j + 1];
                ctx->dir[i][j] = (unsigned char)'L';
            }
        }
        memcpy(prev, curr, sizeof(prev));
    }

    for (i = 0; i <= n; i++) {
        ctx->dir[i][m] = (unsigned char)'U';
    }
    for (j = 0; j <= m; j++) {
        ctx->dir[n][j] = (unsigned char)'L';
    }

    if (n == 0 || m == 0) {
        ctx->lcs_len = 0;
    } else {
        ctx->lcs_len = prev[0];
    }
    if (n == 0 && m == 0) {
        ctx->lcs_len = 0;
    }
}

int diff_has_changes(const diff_ctx_t *ctx) {
    if (ctx->a_count != ctx->b_count) {
        return 1;
    }
    return (ctx->lcs_len != (unsigned char)ctx->a_count);
}

static void print_range(int start, int end, int empty_anchor) {
    if (start > end) {
        printf("%d", empty_anchor);
    } else if (start == end) {
        printf("%d", start);
    } else {
        printf("%d,%d", start, end);
    }
}

void diff_emit_normal(const diff_ctx_t *ctx) {
    int i;
    int j;
    int n;
    int m;

    n = ctx->a_count;
    m = ctx->b_count;
    i = 0;
    j = 0;

    while (i < n || j < m) {
        int ai;
        int bj;
        int a_start;
        int b_start;
        int a_end;
        int b_end;
        int k;

        if (i < n && j < m && lines_equal(ctx->a[i], ctx->b[j])) {
            i++;
            j++;
            continue;
        }

        ai = i;
        bj = j;
        a_start = ai + 1;
        b_start = bj + 1;

        while (i < n || j < m) {
            if (i < n && j < m && lines_equal(ctx->a[i], ctx->b[j])) {
                break;
            }

            if (i >= n) {
                j++;
            } else if (j >= m) {
                i++;
            } else if (ctx->dir[i][j] == (unsigned char)'L') {
                j++;
            } else {
                i++;
            }
        }

        a_end = i;
        b_end = j;

        if (a_start > a_end) {
            print_range(a_start, a_end, a_end);
            printf("a");
            print_range(b_start, b_end, b_end);
            printf("\n");
            for (k = b_start; k <= b_end; k++) {
                printf("> %s\n", ctx->b[k - 1]);
            }
        } else if (b_start > b_end) {
            print_range(a_start, a_end, a_end);
            printf("d");
            print_range(b_start, b_end, b_end);
            printf("\n");
            for (k = a_start; k <= a_end; k++) {
                printf("< %s\n", ctx->a[k - 1]);
            }
        } else {
            print_range(a_start, a_end, a_end);
            printf("c");
            print_range(b_start, b_end, b_end);
            printf("\n");
            for (k = a_start; k <= a_end; k++) {
                printf("< %s\n", ctx->a[k - 1]);
            }
            printf("---\n");
            for (k = b_start; k <= b_end; k++) {
                printf("> %s\n", ctx->b[k - 1]);
            }
        }
    }
}
