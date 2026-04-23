#include "deltree.h"

static void print_usage(void) {
    printf("Sprinter DELTREE %s\r\n", DELTREE_VERSION);
    printf("Author: Dmitry Mikhalchenkov\r\n");
    printf("Usage: deltree [/Y|-Y] DIRECTORY [DIRECTORY ...]\r\n");
    printf("Options:\r\n");
    printf("  /Y,-Y     delete without confirmation\r\n");
    printf("  -H,-h,/?  show this help\r\n");
}

static int parse_opts(deltree_opts_t *opts) {
    char cmdline[MAX_CMDLINE];
    char *argv[MAX_ARGV];
    int argc;
    int i;
    int paths;

    memset(opts, 0, sizeof(deltree_opts_t));
    paths = 0;

    util_read_cmdline_safe(cmdline, sizeof(cmdline));
    argc = util_parse_cmdline(cmdline, argv);

    for (i = 0; i < argc; i++) {
        if (util_streq(argv[i], "-H") || util_streq(argv[i], "-h") || util_streq(argv[i], "/?")) {
            opts->show_help = 1;
            continue;
        }
        if (util_strieq(argv[i], "/Y") || util_strieq(argv[i], "-Y")) {
            opts->assume_yes = 1;
            continue;
        }

        if ((argv[i][0] == '/' || argv[i][0] == '-') && argv[i][1] != '\0') {
            printf("deltree: unsupported option '%s'\r\n", argv[i]);
            return 0;
        }

        if (paths >= MAX_TARGETS) {
            printf("deltree: too many target paths (max %d)\r\n", MAX_TARGETS);
            return 0;
        }

        if (!util_copy_path(opts->paths[paths], sizeof(opts->paths[paths]), argv[i])) {
            printf("deltree: path too long\r\n");
            return 0;
        }
        util_normalize_path(opts->paths[paths]);
        paths++;
    }
    opts->path_count = (unsigned char)paths;

    if (!opts->show_help && paths == 0) {
        printf("deltree: missing directory operand\r\n");
        return 0;
    }
    return 1;
}

void main(void) {
    deltree_opts_t opts;
    int i;
    int exit_code;
    int any_runtime_error;

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

    any_runtime_error = 0;
    exit_code = 0;
    for (i = 0; i < (int)opts.path_count; i++) {
        const char *path;
        u8 attr;
        u8 err_code;
        int is_dir;
        int aborted;
        char err[MAX_TEXT];

        path = opts.paths[i];
        if (path[0] == '\0') {
            continue;
        }

        if (util_has_wildcards(path)) {
            printf("deltree: %s: wildcard paths are not supported in this stage\r\n", path);
            exit_code = 2;
            continue;
        }

        if (util_is_root_path(path)) {
            printf("deltree: refusing to delete root directory '%s'\r\n", path);
            exit_code = 2;
            continue;
        }

        if (!fs_probe_path(path, &attr, &is_dir, &err_code)) {
            printf("deltree: %s: path not found\r\n", path);
            any_runtime_error = 1;
            continue;
        }

        if (!is_dir) {
            printf("deltree: %s: not a directory\r\n", path);
            any_runtime_error = 1;
            continue;
        }

        if (!opts.assume_yes) {
            if (!input_confirm_delete(path, &aborted)) {
                if (aborted) {
                    printf("deltree: operation canceled by user\r\n");
                    dss_exit(1);
                    return;
                }
                continue;
            }
        }

        err[0] = '\0';
        if (!fs_delete_tree(path, err, sizeof(err))) {
            if (err[0] != '\0') {
                printf("%s\r\n", err);
            } else {
                printf("deltree: unknown error\r\n");
            }
            any_runtime_error = 1;
            continue;
        }
    }

    if (exit_code == 0 && any_runtime_error) {
        exit_code = 1;
    }
    dss_exit((u8)exit_code);
}
