#include "diff.h"

static void print_usage(void) {
    printf("Sprinter diff %s\n", DIFF_VERSION);
    printf("Usage: diff [OPTION]... FILE1 FILE2\n");
    printf("Options:\n");
    printf("  -q        report only whether files differ\n");
    printf("  -s        report when two files are the same\n");
    printf("  -H,-h,/?  show this help\n");
}

static int is_safe_arg_char(char c) {
    unsigned char uc;

    uc = (unsigned char)c;
    if ((uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z') || (uc >= '0' && uc <= '9')) {
        return 1;
    }
    if (uc == '.' || uc == '_' || uc == '-' || uc == '/' || uc == '\\' || uc == ':') {
        return 1;
    }
    return 0;
}

static void read_cmdline_safe(char *out, int out_sz) {
    char *src;
    int i;
    int saw_bad;

    src = dss_cmdline();
    i = 0;
    saw_bad = 0;

    while (*src != '\0' && *src != '\r' && *src != '\n' && i < out_sz - 1) {
        unsigned char ch;
        ch = (unsigned char)*src;
        if (ch >= 32 && ch <= 126) {
            out[i++] = (char)ch;
        } else {
            saw_bad = 1;
            break;
        }
        src++;
    }
    out[i] = '\0';

    if (saw_bad) {
        out[0] = '\0';
    }
}

static int parse_opts(diff_opts_t *opts) {
    char cmdline[MAX_CMDLINE];
    char *argv[MAX_ARGV];
    int argc;
    int i;
    int files;

    memset(opts, 0, sizeof(diff_opts_t));
    opts->left[0] = '\0';
    opts->right[0] = '\0';

    read_cmdline_safe(cmdline, sizeof(cmdline));
    argc = util_parse_cmdline(cmdline, argv);

    files = 0;
    for (i = 0; i < argc; i++) {
        if (!util_streq(argv[i], "-q") && !util_streq(argv[i], "-s") && !util_streq(argv[i], "-H") && !util_streq(argv[i], "-h") && !util_streq(argv[i], "/?")) {
            int j;
            int bad;
            bad = 0;
            for (j = 0; argv[i][j] != '\0'; j++) {
                if (!is_safe_arg_char(argv[i][j])) {
                    bad = 1;
                    break;
                }
            }
            if (bad) {
                printf("diff: suspicious argument '%s'\n", argv[i]);
                return 0;
            }
        }

        if (util_streq(argv[i], "-H") || util_streq(argv[i], "-h") || util_streq(argv[i], "/?")) {
            opts->show_help = 1;
        } else if (util_streq(argv[i], "-q")) {
            opts->brief = 1;
        } else if (util_streq(argv[i], "-s")) {
            opts->report_identical = 1;
        } else if (argv[i][0] == '-') {
            printf("diff: unrecognized option '%s'\n", argv[i]);
            return 0;
        } else {
            if (files == 0) {
                strncpy(opts->left, argv[i], sizeof(opts->left) - 1);
                opts->left[sizeof(opts->left) - 1] = '\0';
                files++;
            } else if (files == 1) {
                strncpy(opts->right, argv[i], sizeof(opts->right) - 1);
                opts->right[sizeof(opts->right) - 1] = '\0';
                files++;
            } else {
                printf("diff: extra operand '%s'\n", argv[i]);
                return 0;
            }
        }
    }

    if (!opts->show_help && files != 2) {
        printf("diff: missing file operand\n");
        return 0;
    }

    return 1;
}

static diff_ctx_t g_ctx;

void main(void) {
    diff_opts_t opts;
    int has_diff;
    char err[MAX_TEXT];

    ctx_init(&g_ctx);

    if (!parse_opts(&opts)) {
        print_usage();
        dss_exit(2);
        return;
    }

    if (opts.show_help) {
        print_usage();
        dss_exit(0);
        return;
    }

    err[0] = '\0';
    if (!read_text_lines(&g_ctx, opts.left, g_ctx.a, &g_ctx.a_count, err, sizeof(err))) {
        printf("diff: %s\n", err);
        dss_exit(2);
        return;
    }

    if (!read_text_lines(&g_ctx, opts.right, g_ctx.b, &g_ctx.b_count, err, sizeof(err))) {
        printf("diff: %s\n", err);
        dss_exit(2);
        return;
    }

    diff_build_lcs(&g_ctx);
    has_diff = diff_has_changes(&g_ctx);

    if (opts.brief) {
        if (has_diff) {
            printf("Files %s and %s differ\n", opts.left, opts.right);
        } else if (opts.report_identical) {
            printf("Files %s and %s are identical\n", opts.left, opts.right);
        }
        dss_exit((unsigned char)(has_diff ? 1 : 0));
        return;
    }

    if (has_diff) {
        diff_emit_normal(&g_ctx);
        dss_exit(1);
        return;
    }

    if (opts.report_identical) {
        printf("Files %s and %s are identical\n", opts.left, opts.right);
    }

    dss_exit(0);
}
