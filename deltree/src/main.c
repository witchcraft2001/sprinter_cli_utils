#include "deltree.h"

static void print_usage(void) {
    printf("Sprinter DELTREE %s\r\n", DELTREE_VERSION);
    printf("Author: Dmitry Mikhalchenkov\r\n");
    printf("Usage: deltree DIRECTORY\r\n");
    printf("Options:\r\n");
    printf("  -H,-h,/?  show this help\r\n");
}

static int parse_opts(deltree_opts_t *opts) {
    char cmdline[MAX_CMDLINE];
    char *argv[MAX_ARGV];
    int argc;
    int i;
    int paths;

    memset(opts, 0, sizeof(deltree_opts_t));
    opts->path[0] = '\0';
    paths = 0;

    util_read_cmdline_safe(cmdline, sizeof(cmdline));
    argc = util_parse_cmdline(cmdline, argv);

    for (i = 0; i < argc; i++) {
        if (util_streq(argv[i], "-H") || util_streq(argv[i], "-h") || util_streq(argv[i], "/?")) {
            opts->show_help = 1;
            continue;
        }

        if ((argv[i][0] == '/' || argv[i][0] == '-') && argv[i][1] != '\0') {
            printf("deltree: unsupported option '%s'\r\n", argv[i]);
            return 0;
        }

        if (paths > 0) {
            printf("deltree: extra operand '%s'\r\n", argv[i]);
            return 0;
        }

        if (!util_copy_path(opts->path, sizeof(opts->path), argv[i])) {
            printf("deltree: path too long\r\n");
            return 0;
        }
        paths++;
    }

    if (!opts->show_help && paths != 1) {
        printf("deltree: missing directory operand\r\n");
        return 0;
    }

    util_normalize_path(opts->path);
    return 1;
}

void main(void) {
    deltree_opts_t opts;
    u8 attr;
    u8 err_code;
    int is_dir;
    int aborted;
    char err[MAX_TEXT];

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

    if (opts.path[0] == '\0') {
        print_usage();
        dss_exit(2);
        return;
    }

    if (util_has_wildcards(opts.path)) {
        printf("deltree: wildcard paths are not supported in this stage\r\n");
        dss_exit(2);
        return;
    }

    if (util_is_root_path(opts.path)) {
        printf("deltree: refusing to delete root directory '%s'\r\n", opts.path);
        dss_exit(2);
        return;
    }

    if (!fs_probe_path(opts.path, &attr, &is_dir, &err_code)) {
        printf("deltree: %s: path not found\r\n", opts.path);
        dss_exit(1);
        return;
    }

    if (!is_dir) {
        printf("deltree: %s: not a directory\r\n", opts.path);
        dss_exit(1);
        return;
    }

    if (!input_confirm_delete(opts.path, &aborted)) {
        if (aborted) {
            printf("deltree: operation canceled by user\r\n");
            dss_exit(1);
        } else {
            dss_exit(0);
        }
        return;
    }

    err[0] = '\0';
    if (!fs_delete_tree(opts.path, err, sizeof(err))) {
        if (err[0] != '\0') {
            printf("%s\r\n", err);
        } else {
            printf("deltree: unknown error\r\n");
        }
        dss_exit(1);
        return;
    }

    dss_exit(0);
}
