#include "deltree.h"

#define MAX_DOS_NAME 13

static void print_usage(void) {
    printf("Sprinter DELTREE %s\r\n", DELTREE_VERSION);
    printf("Author: Dmitry Mikhalchenkov\r\n");
    printf("Usage: deltree [/Y|-Y] PATH [PATH ...]\r\n");
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
        printf("deltree: missing path operand\r\n");
        return 0;
    }
    return 1;
}

static const char *path_last_component(const char *path) {
    const char *last;

    last = path;
    while (*path != '\0') {
        if (*path == '\\' || *path == '/' || *path == ':') {
            last = path + 1;
        }
        path++;
    }
    return last;
}

static int wildcards_in_last_component_only(const char *path) {
    const char *leaf;

    leaf = path_last_component(path);
    while (path < leaf) {
        if (*path == '*' || *path == '?') {
            return 0;
        }
        path++;
    }
    return 1;
}

static int split_wildcard_spec(const char *spec,
                               char *base_dir,
                               int base_dir_sz,
                               char *mask,
                               int mask_sz) {
    const char *leaf;
    int base_len;

    leaf = path_last_component(spec);
    if (leaf[0] == '\0') {
        return 0;
    }

    base_len = (int)(leaf - spec);
    if (base_len == 0) {
        if (!util_copy_path(base_dir, base_dir_sz, ".")) {
            return 0;
        }
    } else {
        if (base_len >= base_dir_sz) {
            return 0;
        }
        memcpy(base_dir, spec, (unsigned int)base_len);
        base_dir[base_len] = '\0';
        util_normalize_path(base_dir);
        if (base_dir[0] == '\0') {
            if (!util_copy_path(base_dir, base_dir_sz, ".")) {
                return 0;
            }
        }
    }

    if (!util_copy_path(mask, mask_sz, leaf)) {
        return 0;
    }
    return 1;
}

static int skip_contains(char skip_names[MAX_WILDCARD_SKIPS][MAX_DOS_NAME], int skip_count, const char *name) {
    int i;

    for (i = 0; i < skip_count; i++) {
        if (util_strieq(skip_names[i], name)) {
            return 1;
        }
    }
    return 0;
}

static int skip_add(char skip_names[MAX_WILDCARD_SKIPS][MAX_DOS_NAME], int *skip_count, const char *name) {
    if (*skip_count >= MAX_WILDCARD_SKIPS) {
        return 0;
    }
    if (!util_copy_path(skip_names[*skip_count], MAX_DOS_NAME, name)) {
        return 0;
    }
    (*skip_count)++;
    return 1;
}

static int find_next_wildcard_match(const char *spec,
                                    const char *base_dir,
                                    char skip_names[MAX_WILDCARD_SKIPS][MAX_DOS_NAME],
                                    int skip_count,
                                    char *match_path,
                                    int match_path_sz,
                                    char *match_name,
                                    int match_name_sz,
                                    u8 *match_attr,
                                    int *found,
                                    char *err,
                                    int err_sz) {
    ffblk entry;
    int rc;

    *found = 0;
    rc = findfirst(spec, &entry, 0x3F);
    if (rc != 0) {
        return 1;
    }

    do {
        if (util_is_dot_entry(entry.ff_name)) {
            continue;
        }
        if ((entry.attr & FA_LABEL) != 0u) {
            continue;
        }

        if (skip_contains(skip_names, skip_count, entry.ff_name)) {
            continue;
        }
        if (!util_copy_path(match_name, match_name_sz, entry.ff_name)) {
            sprintf(err, "deltree: %s\\%s: name too long", base_dir, entry.ff_name);
            (void)err_sz;
            return 0;
        }

        if (!util_join_path(match_path, match_path_sz, base_dir, match_name)) {
            sprintf(err, "deltree: %s\\%s: path too long", base_dir, entry.ff_name);
            (void)err_sz;
            return 0;
        }

        *match_attr = entry.attr;
        *found = 1;
        return 1;
    } while (findnext(&entry) == 0);

    return 1;
}

static int delete_file_target(const char *path,
                              u8 attr,
                              unsigned char assume_yes,
                              int *aborted,
                              char *err,
                              int err_sz) {
    if (!assume_yes) {
        if (!input_confirm_file(path, aborted)) {
            return 1;
        }
    }

    if (!fs_delete_file_known_attr(path, attr, err, err_sz)) {
        return 0;
    }

    return 1;
}

static int delete_dir_target(const char *path,
                             unsigned char assume_yes,
                             int *aborted,
                             char *err,
                             int err_sz) {
    if (!assume_yes) {
        if (!input_confirm_delete(path, aborted)) {
            return 1;
        }
    }

    if (!fs_delete_tree(path, err, err_sz)) {
        return 0;
    }

    return 1;
}

static void mark_runtime_error_from_text(const char *err, int *runtime_error, int *aborted) {
    if (err[0] == '\0') {
        return;
    }
    if (util_strieq(err, "deltree: Aborted")) {
        *aborted = 1;
        return;
    }
    *runtime_error = 1;
}

static int process_plain_target(const char *path,
                                unsigned char assume_yes,
                                int *runtime_error,
                                int *aborted) {
    u8 attr;
    u8 err_code;
    int is_dir;
    int ask_aborted;
    char err[MAX_TEXT];

    if (util_is_root_path(path)) {
        printf("deltree: refusing to delete root directory '%s'\r\n", path);
        return 2;
    }

    if (!fs_probe_path(path, &attr, &is_dir, &err_code)) {
        printf("deltree: %s: path not found\r\n", path);
        *runtime_error = 1;
        return 0;
    }

    ask_aborted = 0;
    err[0] = '\0';
    if (is_dir) {
        if (!delete_dir_target(path, assume_yes, &ask_aborted, err, sizeof(err))) {
            mark_runtime_error_from_text(err, runtime_error, aborted);
            if (*aborted) {
                return 1;
            }
            if (err[0] != '\0') {
                printf("%s\r\n", err);
            } else {
                printf("deltree: unknown error\r\n");
            }
            *runtime_error = 1;
            return 0;
        }
    } else {
        if (!delete_file_target(path, attr, assume_yes, &ask_aborted, err, sizeof(err))) {
            mark_runtime_error_from_text(err, runtime_error, aborted);
            if (*aborted) {
                return 1;
            }
            if (err[0] != '\0') {
                printf("%s\r\n", err);
            } else {
                printf("deltree: unknown error\r\n");
            }
            *runtime_error = 1;
            return 0;
        }
    }

    if (ask_aborted) {
        *aborted = 1;
    }
    return 0;
}

static int process_wildcard_target(const char *spec,
                                   unsigned char assume_yes,
                                   int *runtime_error,
                                   int *aborted) {
    char base_dir[MAX_PATH_TEXT];
    char mask[MAX_PATH_TEXT];
    char match_path[MAX_PATH_TEXT];
    char match_name[MAX_DOS_NAME];
    char err[MAX_TEXT];
    char skip_names[MAX_WILDCARD_SKIPS][MAX_DOS_NAME];
    int skip_count;
    int found_any;

    if (!wildcards_in_last_component_only(spec)) {
        printf("deltree: %s: wildcard allowed only in last path component\r\n", spec);
        return 2;
    }

    if (!split_wildcard_spec(spec, base_dir, sizeof(base_dir), mask, sizeof(mask))) {
        printf("deltree: %s: bad wildcard path\r\n", spec);
        return 2;
    }

    if (mask[0] == '\0') {
        printf("deltree: %s: empty wildcard mask\r\n", spec);
        return 2;
    }

    skip_count = 0;
    found_any = 0;

    while (1) {
        u8 match_attr;
        int found;
        int ask_aborted;

        err[0] = '\0';
        if (!find_next_wildcard_match(spec,
                                      base_dir,
                                      skip_names,
                                      skip_count,
                                      match_path,
                                      sizeof(match_path),
                                      match_name,
                                      sizeof(match_name),
                                      &match_attr,
                                      &found,
                                      err,
                                      sizeof(err))) {
            printf("%s\r\n", err);
            *runtime_error = 1;
            return 0;
        }

        if (!found) {
            break;
        }
        found_any = 1;

        ask_aborted = 0;
        err[0] = '\0';

        if ((match_attr & FA_DIREC) != 0u) {
            if (util_is_root_path(match_path)) {
                if (!skip_add(skip_names, &skip_count, match_name)) {
                    printf("deltree: %s: too many skipped wildcard matches\r\n", spec);
                    *runtime_error = 1;
                    return 0;
                }
                continue;
            }

            if (!delete_dir_target(match_path, assume_yes, &ask_aborted, err, sizeof(err))) {
                if (err[0] != '\0') {
                    mark_runtime_error_from_text(err, runtime_error, aborted);
                    if (*aborted) {
                        return 0;
                    }
                    printf("%s\r\n", err);
                    *runtime_error = 1;
                }

                if (ask_aborted) {
                    *aborted = 1;
                    return 0;
                }

                if (!skip_add(skip_names, &skip_count, match_name)) {
                    printf("deltree: %s: too many skipped wildcard matches\r\n", spec);
                    *runtime_error = 1;
                    return 0;
                }
            } else if (ask_aborted) {
                *aborted = 1;
                return 0;
            }
            continue;
        }

        if (!delete_file_target(match_path, match_attr, assume_yes, &ask_aborted, err, sizeof(err))) {
            if (err[0] != '\0') {
                mark_runtime_error_from_text(err, runtime_error, aborted);
                if (*aborted) {
                    return 0;
                }
                printf("%s\r\n", err);
                *runtime_error = 1;
            }

            if (ask_aborted) {
                *aborted = 1;
                return 0;
            }

            if (!skip_add(skip_names, &skip_count, match_name)) {
                printf("deltree: %s: too many skipped wildcard matches\r\n", spec);
                *runtime_error = 1;
                return 0;
            }
        } else if (ask_aborted) {
            *aborted = 1;
            return 0;
        }
    }

    if (!found_any) {
        printf("deltree: %s: no matches\r\n", spec);
    }

    return 0;
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
        int aborted;
        int rc;

        path = opts.paths[i];
        if (path[0] == '\0') {
            continue;
        }

        aborted = 0;
        if (util_has_wildcards(path)) {
            rc = process_wildcard_target(path, opts.assume_yes, &any_runtime_error, &aborted);
        } else {
            rc = process_plain_target(path, opts.assume_yes, &any_runtime_error, &aborted);
        }

        if (rc == 2) {
            exit_code = 2;
            continue;
        }

        if (aborted) {
            printf("deltree: Aborted\r\n");
            dss_exit(1);
            return;
        }
    }

    if (exit_code == 0 && any_runtime_error) {
        exit_code = 1;
    }

    dss_exit((u8)exit_code);
}
