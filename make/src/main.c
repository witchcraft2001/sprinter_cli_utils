#include "make.h"

static void print_usage(void) {
    printf("Sprinter make %s\n", MAKE_VERSION);
    printf("Author: Dmitry Mikhalchenkov\n");
    printf("Usage: make [-n] [-f FILE] [target]\n");
    printf("       make /?\n");
    printf("Options:\n");
    printf("  -f FILE   use specified makefile\n");
    printf("  -n        dry-run (print commands only)\n");
    printf("  -H,-h,/?  show this help\n");
}

static int is_safe_arg_char(char c) {
    unsigned char uc;

    uc = (unsigned char)c;
    if ((uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z') || (uc >= '0' && uc <= '9')) {
        return 1;
    }
    if (uc == '.' || uc == '_' || uc == '-' || uc == '/' || uc == '\\' || uc == ':' || uc == '+') {
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

static int parse_opts(make_opts_t *opts, int *show_help) {
    char cmdline[MAX_CMDLINE];
    char *argv[MAX_ARGV];
    int argc;
    int i;

    strcpy(opts->makefile, "Makefile");
    opts->goal[0] = '\0';
    opts->has_goal = 0;
    opts->dry_run = 0;
    *show_help = 0;

    MAKE_STAGE("parse_opts: read cmdline");
    read_cmdline_safe(cmdline, sizeof(cmdline));

    MAKE_LOG("make: raw cmdline='%s'\n", cmdline);
    MAKE_STAGE("parse_opts: tokenize cmdline");
    argc = util_parse_cmdline(cmdline, argv);
    MAKE_LOG("make: argc=%d\n", argc);
    for (i = 0; i < argc; i++) {
        if (!util_streq(argv[i], "-n") && !util_streq(argv[i], "-f") && !util_streq(argv[i], "-H") && !util_streq(argv[i], "-h") && !util_streq(argv[i], "/?")) {
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
                MAKE_LOG("make: ignore suspicious arg '%s'\n", argv[i]);
                continue;
            }
        }

        if (util_streq(argv[i], "/?") || util_streq(argv[i], "-H") || util_streq(argv[i], "-h")) {
            *show_help = 1;
            return 1;
        } else if (util_streq(argv[i], "-n")) {
            opts->dry_run = 1;
        } else if (util_streq(argv[i], "-f")) {
            if (i + 1 >= argc) {
                printf("make: option -f requires a file name\n");
                return 0;
            }
            strncpy(opts->makefile, argv[++i], sizeof(opts->makefile) - 1);
            opts->makefile[sizeof(opts->makefile) - 1] = '\0';
        } else if (argv[i][0] == '-') {
            printf("make: unknown option '%s'\n", argv[i]);
            return 0;
        } else {
            if (!opts->has_goal) {
                strncpy(opts->goal, argv[i], sizeof(opts->goal) - 1);
                opts->goal[sizeof(opts->goal) - 1] = '\0';
                opts->has_goal = 1;
            } else {
                MAKE_LOG("make: ignore extra goal '%s'\n", argv[i]);
            }
        }
    }

    return 1;
}

static make_ctx_t g_ctx;

void main(void) {
    make_opts_t opts;
    int show_help;
    int goal;
    int rc;
    char cwd[MAX_TEXT];
    int cwd_rc;
    u8 disk;

    printf("Sprinter make %s\n", MAKE_VERSION);
    printf("Author: Dmitry Mikhalchenkov\n");
    MAKE_STAGE("main: banner printed");

    MAKE_STAGE("main: ctx_init start");
    ctx_init(&g_ctx);
    MAKE_STAGE("main: ctx_init done");

    MAKE_STAGE("main: parse_opts start");
    if (!parse_opts(&opts, &show_help)) {
        MAKE_STAGE("main: parse_opts failed");
        print_usage();
        dss_exit(2);
        return;
    }
    MAKE_STAGE("main: parse_opts done");

    if (show_help) {
        MAKE_STAGE("main: show help");
        print_usage();
        dss_exit(0);
        return;
    }

    cwd[0] = '\0';
    cwd_rc = (int)dss_curdir(cwd);
    disk = dss_getdisk();
    if (cwd_rc == 0) {
        MAKE_LOG("make: cwd='%s' disk=%c:\n", cwd, (char)('A' + disk));
    } else {
        MAKE_LOG("make: cwd read failed rc=%d disk=%c:\n", cwd_rc, (char)('A' + disk));
    }

    MAKE_LOG("make: loading file '%s'\n", opts.makefile);

    MAKE_STAGE("main: load makefile");
    if (!parser_load_file(&g_ctx, opts.makefile)) {
        if (util_streq(opts.makefile, "Makefile")) {
            if (!parser_load_file(&g_ctx, "makefile")) {
                if (!parser_load_file(&g_ctx, "MAKEFILE")) {
                    printf("make: cannot open makefile\n");
                    dss_exit(2);
                    return;
                }
            }
        } else {
            printf("make: cannot open '%s'\n", opts.makefile);
            dss_exit(2);
            return;
        }
    }
    MAKE_STAGE("main: makefile loaded");

    if (g_ctx.target_count == 0 || g_ctx.first_target_idx == 0xFF) {
        printf("make: no targets\n");
        dss_exit(2);
        return;
    }

    if (opts.has_goal) {
        MAKE_LOG("make: requested goal '%s'\n", opts.goal);
        goal = find_target(&g_ctx, opts.goal);
        if (goal < 0) {
            printf("make: target '%s' not found\n", opts.goal);
            dss_exit(1);
            return;
        }
    } else {
        goal = (int)g_ctx.first_target_idx;
    }

    MAKE_STAGE("main: build_goal start");
    rc = build_goal(&g_ctx, goal, &opts);
    MAKE_STAGE("main: build_goal done");
    if (rc != 0) {
        dss_exit((unsigned char)1);
        return;
    }

    if (!g_ctx.did_work) {
        printf("make: Nothing to be done for `%s'.\n", g_ctx.targets[goal].name);
    }

    dss_exit(0);
}
