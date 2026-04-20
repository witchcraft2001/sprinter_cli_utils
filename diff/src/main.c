#include "diff.h"

static void print_usage(void) {
    printf("Sprinter diff %s\n", DIFF_VERSION);
    printf("Author: Dmitry Mikhalchenkov\n");
    printf("Usage: diff [OPTION]... FILE1 FILE2\n");
    printf("Options:\n");
    printf("  -q        report only whether files differ\n");
    printf("  -s        report when two files are the same\n");
    printf("  -a        treat all files as text\n");
    printf("  -i        ignore case differences\n");
    printf("  -b        ignore changes in amount of spaces/tabs\n");
    printf("  -w        ignore all spaces/tabs\n");
    printf("  -u        output unified diff (3 lines context)\n");
    printf("  -U N      output unified diff with N context lines\n");
    printf("  -o FILE   write diff output to FILE\n");
    printf("  -H,-h,/?  show this help\n");
}

static int parse_uint(const char *s, int *out) {
    int v;

    if (*s == '\0') {
        return 0;
    }
    v = 0;
    while (*s != '\0') {
        if (*s < '0' || *s > '9') {
            return 0;
        }
        v = v * 10 + (*s - '0');
        if (v > 255) {
            return 0;
        }
        s++;
    }
    *out = v;
    return 1;
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
    opts->mode = DIFF_MODE_NORMAL;
    opts->unified_context = 3;
    opts->out_path[0] = '\0';
    opts->left[0] = '\0';
    opts->right[0] = '\0';

    read_cmdline_safe(cmdline, sizeof(cmdline));
    argc = util_parse_cmdline(cmdline, argv);

    files = 0;
    for (i = 0; i < argc; i++) {
        if (!util_streq(argv[i], "-q") && !util_streq(argv[i], "-s") && !util_streq(argv[i], "-a") && !util_streq(argv[i], "-i") && !util_streq(argv[i], "-b") && !util_streq(argv[i], "-w") && !util_streq(argv[i], "-u") && !util_streq(argv[i], "-U") && !util_streq(argv[i], "-o") && !util_streq(argv[i], "-H") && !util_streq(argv[i], "-h") && !util_streq(argv[i], "/?") && !(argv[i][0] == '-' && argv[i][1] == 'U' && argv[i][2] != '\0')) {
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
        } else if (util_streq(argv[i], "-a")) {
            opts->force_text = 1;
        } else if (util_streq(argv[i], "-i")) {
            opts->ignore_case = 1;
        } else if (util_streq(argv[i], "-b")) {
            opts->ignore_space_change = 1;
        } else if (util_streq(argv[i], "-w")) {
            opts->ignore_all_space = 1;
        } else if (util_streq(argv[i], "-u")) {
            opts->mode = DIFF_MODE_UNIFIED;
            opts->unified_context = 3;
        } else if (util_streq(argv[i], "-U")) {
            int n;

            if (i + 1 >= argc) {
                printf("diff: option -U requires a number\n");
                return 0;
            }
            if (!parse_uint(argv[++i], &n)) {
                printf("diff: invalid context length '%s'\n", argv[i]);
                return 0;
            }
            opts->mode = DIFF_MODE_UNIFIED;
            if (n > MAX_UNIFIED_CONTEXT) {
                printf("diff: context too large (max %d)\n", MAX_UNIFIED_CONTEXT);
                return 0;
            }
            opts->unified_context = (unsigned char)n;
        } else if (argv[i][0] == '-' && argv[i][1] == 'U' && argv[i][2] != '\0') {
            int n;

            if (!parse_uint(argv[i] + 2, &n)) {
                printf("diff: invalid context length '%s'\n", argv[i] + 2);
                return 0;
            }
            opts->mode = DIFF_MODE_UNIFIED;
            if (n > MAX_UNIFIED_CONTEXT) {
                printf("diff: context too large (max %d)\n", MAX_UNIFIED_CONTEXT);
                return 0;
            }
            opts->unified_context = (unsigned char)n;
        } else if (util_streq(argv[i], "-o")) {
            if (i + 1 >= argc) {
                printf("diff: option -o requires a file name\n");
                return 0;
            }
            strncpy(opts->out_path, argv[++i], sizeof(opts->out_path) - 1);
            opts->out_path[sizeof(opts->out_path) - 1] = '\0';
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
    FILE *out_fp;
    int left_binary;
    int right_binary;
    int same_binary;
    int same_exact;
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

    if (opts.out_path[0] != '\0') {
        if (util_streq(opts.out_path, opts.left) || util_streq(opts.out_path, opts.right)) {
            printf("diff: output file must differ from input files\n");
            dss_exit(2);
            return;
        }
    }

    out_fp = stdout;
    if (!opts.brief && opts.out_path[0] != '\0') {
        out_fp = fopen(opts.out_path, "w");
        if (out_fp == (FILE *)0) {
            printf("diff: cannot open output '%s'\n", opts.out_path);
            dss_exit(2);
            return;
        }
    }
    g_ctx.out = out_fp;

    if (!opts.force_text) {
        if (!diff_probe_binary(opts.left, &left_binary, err, sizeof(err))) {
            if (out_fp != stdout) {
                fclose(out_fp);
            }
            printf("diff: %s\n", err);
            dss_exit(2);
            return;
        }
        if (!diff_probe_binary(opts.right, &right_binary, err, sizeof(err))) {
            if (out_fp != stdout) {
                fclose(out_fp);
            }
            printf("diff: %s\n", err);
            dss_exit(2);
            return;
        }

        if (left_binary || right_binary) {
            if (!diff_compare_binary_files(opts.left, opts.right, &same_binary, err, sizeof(err))) {
                if (out_fp != stdout) {
                    fclose(out_fp);
                }
                printf("diff: %s\n", err);
                dss_exit(2);
                return;
            }

            if (!same_binary) {
                if (opts.brief || out_fp == stdout) {
                    printf("Binary files %s and %s differ\n", opts.left, opts.right);
                } else {
                    fprintf(out_fp, "Binary files %s and %s differ\n", opts.left, opts.right);
                    fclose(out_fp);
                }
                dss_exit(1);
                return;
            }

            if (out_fp != stdout) {
                fclose(out_fp);
            }
            if (opts.report_identical) {
                printf("Files %s and %s are identical\n", opts.left, opts.right);
            }
            dss_exit(0);
            return;
        }
    }

    if (!opts.ignore_case && !opts.ignore_space_change && !opts.ignore_all_space) {
        if (!diff_compare_binary_files(opts.left, opts.right, &same_exact, err, sizeof(err))) {
            if (out_fp != stdout) {
                fclose(out_fp);
            }
            printf("diff: %s\n", err);
            dss_exit(2);
            return;
        }

        if (same_exact) {
            if (out_fp != stdout) {
                fclose(out_fp);
            }
            if (opts.report_identical) {
                printf("Files %s and %s are identical\n", opts.left, opts.right);
            }
            dss_exit(0);
            return;
        }
    }

    err[0] = '\0';
    if (!diff_compare_files(&g_ctx,
                            opts.left,
                            opts.right,
                            (unsigned char)(opts.brief ? 0 : 1),
                            opts.mode,
                            opts.unified_context,
                            opts.ignore_case,
                            opts.ignore_space_change,
                            opts.ignore_all_space,
                            &has_diff,
                            err,
                            sizeof(err))) {
        if (out_fp != stdout) {
            fclose(out_fp);
        }
        if (util_streq(err, "Aborted")) {
            printf("Aborted\n");
        } else {
            printf("diff: %s\n", err);
        }
        dss_exit(2);
        return;
    }

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
        if (out_fp != stdout) {
            fclose(out_fp);
        }
        dss_exit(1);
        return;
    }

    if (out_fp != stdout) {
        fclose(out_fp);
    }

    if (opts.report_identical) {
        printf("Files %s and %s are identical\n", opts.left, opts.right);
    }

    dss_exit(0);
}
