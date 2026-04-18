#include "make.h"

static void print_usage(void) {
    printf("Sprinter make (MVP)\n");
    printf("Usage: make [-n] [-f FILE] [target]\n");
}

static int parse_opts(make_opts_t *opts) {
    char cmdline[MAX_CMDLINE];
    char *argv[MAX_ARGV];
    char *src;
    int argc;
    int i;

    opts->makefile = "Makefile";
    opts->goal = (const char *)0;
    opts->dry_run = 0;

    src = dss_cmdline();
    strncpy(cmdline, src, sizeof(cmdline) - 1);
    cmdline[sizeof(cmdline) - 1] = '\0';

    argc = util_parse_cmdline(cmdline, argv);
    for (i = 0; i < argc; i++) {
        if (util_streq(argv[i], "-n")) {
            opts->dry_run = 1;
        } else if (util_streq(argv[i], "-f")) {
            if (i + 1 >= argc) {
                printf("make: option -f requires a file name\n");
                return 0;
            }
            opts->makefile = argv[++i];
        } else if (argv[i][0] == '-') {
            printf("make: unknown option '%s'\n", argv[i]);
            return 0;
        } else {
            opts->goal = argv[i];
        }
    }

    return 1;
}

static make_ctx_t g_ctx;

void main(void) {
    make_opts_t opts;
    int goal;
    int rc;

    ctx_init(&g_ctx);

    if (!parse_opts(&opts)) {
        print_usage();
        dss_exit(2);
        return;
    }

    if (!parser_load_file(&g_ctx, opts.makefile)) {
        if (util_streq(opts.makefile, "Makefile")) {
            if (!parser_load_file(&g_ctx, "makefile")) {
                printf("make: cannot open makefile\n");
                dss_exit(2);
                return;
            }
        } else {
            printf("make: cannot open '%s'\n", opts.makefile);
            dss_exit(2);
            return;
        }
    }

    if (g_ctx.target_count == 0 || g_ctx.first_target_idx == 0xFF) {
        printf("make: no targets\n");
        dss_exit(2);
        return;
    }

    if (opts.goal != (const char *)0) {
        goal = find_target(&g_ctx, opts.goal);
        if (goal < 0) {
            printf("make: target '%s' not found\n", opts.goal);
            dss_exit(1);
            return;
        }
    } else {
        goal = (int)g_ctx.first_target_idx;
    }

    rc = build_goal(&g_ctx, goal, &opts);
    if (rc != 0) {
        dss_exit((unsigned char)1);
        return;
    }

    dss_exit(0);
}
