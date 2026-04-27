#include "xcopy.h"

static void print_usage(void) {
    printf("Sprinter xcopy %s\r\n", XCOPY_VERSION);
    printf("Author: Dmitry Mikhalchenkov\r\n");
    printf("Usage: xcopy SOURCE DEST [/Y] [/-Y] [/E] [/H] [/K] [/V]\r\n");
    printf("Options:\r\n");
    printf("  /Y        overwrite existing files without prompt\r\n");
    printf("  /-Y       always ask before overwrite\r\n");
    printf("  /E        copy empty directories too\r\n");
    printf("  /H        include hidden/system files and directories\r\n");
    printf("  /K        preserve file attributes\r\n");
    printf("  /V        verbose progress while scanning directories\r\n");
    printf("  /?        show this help\r\n");
}

static int is_safe_arg_char(char c) {
    unsigned char uc;

    uc = (unsigned char)c;
    if ((uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z') || (uc >= '0' && uc <= '9')) {
        return 1;
    }
    if (uc == '.' || uc == '_' || uc == '-' || uc == '/' || uc == '\\' || uc == ':' || uc == '*' || uc == '?') {
        return 1;
    }
    return 0;
}

static int contains_text(const char *text, const char *needle) {
    int i;

    if (*needle == '\0') {
        return 1;
    }

    while (*text != '\0') {
        i = 0;
        while (needle[i] != '\0' && text[i] != '\0' && text[i] == needle[i]) {
            i++;
        }
        if (needle[i] == '\0') {
            return 1;
        }
        text++;
    }

    return 0;
}

static void apply_copycmd_defaults(xcopy_opts_t *opts) {
    char *copycmd;

    if (opts->force_prompt || opts->assume_yes) {
        return;
    }
    copycmd = getenv("COPYCMD");
    if (copycmd == (char *)0) {
        return;
    }
    if (contains_text(copycmd, "/-Y")) {
        opts->force_prompt = 1u;
        return;
    }
    if (contains_text(copycmd, "/Y")) {
        opts->assume_yes = 1u;
    }
}

static int parse_opts(xcopy_opts_t *opts) {
    char cmdline[MAX_CMDLINE];
    char *argv[MAX_ARGV];
    int argc;
    int i;
    int files;

    memset(opts, 0, sizeof(xcopy_opts_t));
    opts->src[0] = '\0';
    opts->dst[0] = '\0';

    util_read_cmdline_safe(cmdline, sizeof(cmdline));
    argc = util_parse_cmdline(cmdline, argv);

    files = 0;
    for (i = 0; i < argc; i++) {
        int j;

        for (j = 0; argv[i][j] != '\0'; j++) {
            if (!is_safe_arg_char(argv[i][j])) {
                printf("xcopy: suspicious argument '%s'\r\n", argv[i]);
                return 0;
            }
        }

        if (util_strieq(argv[i], "/?") || util_strieq(argv[i], "-H") || util_strieq(argv[i], "-h")) {
            opts->show_help = 1u;
        } else if (util_strieq(argv[i], "/Y")) {
            opts->assume_yes = 1u;
            opts->force_prompt = 0u;
        } else if (util_strieq(argv[i], "/-Y")) {
            opts->force_prompt = 1u;
            opts->assume_yes = 0u;
        } else if (util_strieq(argv[i], "/E")) {
            opts->copy_empty = 1u;
        } else if (util_strieq(argv[i], "/H")) {
            opts->include_hidden = 1u;
        } else if (util_strieq(argv[i], "/K")) {
            opts->preserve_attr = 1u;
        } else if (util_strieq(argv[i], "/V")) {
            opts->verbose = 1u;
        } else if (argv[i][0] == '/' || argv[i][0] == '-') {
            printf("xcopy: unknown option '%s'\r\n", argv[i]);
            return 0;
        } else if (files == 0) {
            if (!util_copy_path(opts->src, sizeof(opts->src), argv[i])) {
                printf("xcopy: source path too long\r\n");
                return 0;
            }
            files++;
        } else if (files == 1) {
            opts->dst_hint_dir = util_has_trailing_sep(argv[i]) ? 1u : 0u;
            if (!util_copy_path(opts->dst, sizeof(opts->dst), argv[i])) {
                printf("xcopy: destination path too long\r\n");
                return 0;
            }
            files++;
        } else {
            printf("xcopy: extra operand '%s'\r\n", argv[i]);
            return 0;
        }
    }

    if (!opts->show_help && files != 2) {
        printf("xcopy: missing SOURCE or DEST\r\n");
        return 0;
    }

    util_normalize_path(opts->src);
    util_normalize_path(opts->dst);
    apply_copycmd_defaults(opts);
    if (opts->force_prompt) {
        opts->assume_yes = 0u;
    }

    return 1;
}

void main(void) {
    xcopy_ctx_t ctx;
    char err[MAX_TEXT];

    memset(&ctx, 0, sizeof(ctx));
    err[0] = '\0';

    printf("Sprinter xcopy %s\r\n", XCOPY_VERSION);

    if (!parse_opts(&ctx.opts)) {
        print_usage();
        dss_exit(2u);
        return;
    }

    if (ctx.opts.show_help) {
        print_usage();
        dss_exit(0u);
        return;
    }

    if (ctx.opts.verbose) {
        printf("Start %s -> %s\r\n", ctx.opts.src, ctx.opts.dst);
    }

    if (!xcopy_run(&ctx, err, sizeof(err))) {
        ctx.had_error = 1u;
        if (err[0] != '\0') {
            printf("%s\r\n", err);
        }
    }

    ctx.end_cs = 0ul;
    xcopy_print_stats(&ctx);
    buffer_free(&ctx.buffer);

    if (ctx.had_error || ctx.aborted) {
        dss_exit(1u);
        return;
    }

    dss_exit(0u);
}
