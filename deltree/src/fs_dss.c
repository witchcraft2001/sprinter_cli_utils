#include "deltree.h"

typedef struct {
    char path[MAX_PATH_TEXT];
} deltree_frame_t;

const char *g_fs_attr_path;
u8 g_fs_attr_mode;
u8 g_fs_attr_value;
u8 g_fs_attr_result;

u8 fs_dss_attrib_call(void) __naked {
    __asm
        push    ix
        ld      hl, (_g_fs_attr_path)
        ld      a, (_g_fs_attr_value)
        ld      a, (_g_fs_attr_mode)
        ld      b, a
        ld      a, (_g_fs_attr_value)
        ld      c, #0x16
        rst     #0x10
        ld      (_g_fs_attr_result), a
        pop     ix
        jr      c, _fs_attr_err
        xor     a
        ret
_fs_attr_err:
        ld      a, #1
        ret
    __endasm;
}

static int fs_dss_getattr(const char *path, u8 *attr, u8 *err_code) {
    g_fs_attr_path = path;
    g_fs_attr_mode = 0;
    g_fs_attr_value = 0;
    if (fs_dss_attrib_call() != 0) {
        if (err_code != (u8 *)0) {
            *err_code = g_fs_attr_result;
        }
        return 0;
    }
    *attr = g_fs_attr_result;
    return 1;
}

static int fs_dss_setattr(const char *path, u8 attr, u8 *err_code) {
    g_fs_attr_path = path;
    g_fs_attr_mode = 1;
    g_fs_attr_value = attr;
    if (fs_dss_attrib_call() != 0) {
        if (err_code != (u8 *)0) {
            *err_code = g_fs_attr_result;
        }
        return 0;
    }
    return 1;
}

static int fs_clear_special_attrs(const char *path,
                                  u8 known_attr,
                                  unsigned char has_known_attr,
                                  char *err,
                                  int err_sz) {
    u8 attr;
    u8 err_code;
    u8 new_attr;

    if (!fs_dss_getattr(path, &attr, &err_code)) {
        if (!has_known_attr) {
            sprintf(err, "deltree: %s: cannot read attributes (err=%u)", path, (unsigned int)err_code);
            (void)err_sz;
            return 0;
        }
        attr = known_attr;
    }

    new_attr = attr & (u8)~(FA_RDONLY | FA_HIDDEN | FA_SYSTEM);
    if (new_attr == attr) {
        return 1;
    }

    if (!fs_dss_setattr(path, new_attr, &err_code)) {
        sprintf(err, "deltree: %s: cannot update attributes (err=%u)", path, (unsigned int)err_code);
        (void)err_sz;
        return 0;
    }

    return 1;
}

int fs_probe_path(const char *path, u8 *attr, int *is_dir, u8 *err_code) {
    ffblk found;
    int rc;

    rc = findfirst(path, &found, 0x3F);
    if (rc != 0) {
        if (err_code != (u8 *)0) {
            *err_code = 1;
        }
        return 0;
    }

    if (attr != (u8 *)0) {
        *attr = found.attr;
    }
    *is_dir = ((found.attr & FA_DIREC) != 0);
    return 1;
}

static int fs_find_first_child(const char *dir_path,
                               char *child_path,
                               int child_path_sz,
                               u8 *child_attr,
                               int *found_child,
                               char *err,
                               int err_sz) {
    static const char *patterns[] = {
        "*.*",
        "*",
        "*.",
        "????????.???"
    };
    char pattern[MAX_PATH_TEXT];
    ffblk entry;
    int rc;
    int pi;

    *found_child = 0;

    for (pi = 0; pi < (int)(sizeof(patterns) / sizeof(patterns[0])); pi++) {
        if (!util_join_path(pattern, sizeof(pattern), dir_path, patterns[pi])) {
            sprintf(err, "deltree: %s: path too long", dir_path);
            (void)err_sz;
            return 0;
        }

        rc = findfirst(pattern, &entry, 0x3F);
        if (rc != 0) {
            continue;
        }

        do {
            if (util_is_dot_entry(entry.ff_name)) {
                continue;
            }
            if ((entry.attr & FA_LABEL) != 0u) {
                continue;
            }

            if (!util_join_path(child_path, child_path_sz, dir_path, entry.ff_name)) {
                sprintf(err, "deltree: %s\\%s: path too long", dir_path, entry.ff_name);
                (void)err_sz;
                return 0;
            }

            *child_attr = entry.attr;
            *found_child = 1;
            return 1;
        } while (findnext(&entry) == 0);
    }

    return 1;
}

static int fs_delete_file_with_attrs(const char *path, u8 known_attr, char *err, int err_sz) {
    u8 rc;

    if (!fs_clear_special_attrs(path, known_attr, 1u, err, err_sz)) {
        return 0;
    }

    rc = dss_delete(path);
    if (rc != 0) {
        sprintf(err, "deltree: %s: cannot remove file (err=%u)", path, (unsigned int)rc);
        (void)err_sz;
        return 0;
    }

    return 1;
}

int fs_delete_file_known_attr(const char *path, u8 attr, char *err, int err_sz) {
    return fs_delete_file_with_attrs(path, attr, err, err_sz);
}

static int fs_try_rmdir_via_parent(const char *path, u8 *rc_out) {
    char cwd[MAX_PATH_TEXT];
    char parent[MAX_PATH_TEXT];
    char leaf[MAX_PATH_TEXT];
    char *sep;

    *rc_out = 0xFF;

    if (dss_curdir(cwd) != 0) {
        return 0;
    }
    if (!util_copy_path(parent, sizeof(parent), path)) {
        return 0;
    }

    sep = strrchr(parent, '\\');
    if (sep == (char *)0) {
        sep = strrchr(parent, '/');
    }
    if (sep == (char *)0 || sep[1] == '\0') {
        return 0;
    }

    if (!util_copy_path(leaf, sizeof(leaf), sep + 1)) {
        return 0;
    }
    *sep = '\0';

    if (parent[0] == '\0') {
        return 0;
    }

    if (dss_chdir(parent) != 0) {
        return 0;
    }

    *rc_out = rmdir(leaf);
    (void)dss_chdir(cwd);
    return (*rc_out == 0);
}

static int fs_delete_dir_with_attrs(const char *path, u8 known_attr, unsigned char has_known, char *err, int err_sz) {
    u8 rc;

    if (!fs_clear_special_attrs(path, known_attr, has_known, err, err_sz)) {
        return 0;
    }

    printf("removing directory: %s\r\n", path);
    rc = rmdir(path);
    if (rc != 0) {
        u8 rc2;
        char child_path[MAX_PATH_TEXT];
        u8 child_attr;
        int found_child;
        char scan_err[MAX_TEXT];

        if (fs_try_rmdir_via_parent(path, &rc2)) {
            return 1;
        }

        scan_err[0] = '\0';
        if (fs_find_first_child(path,
                                child_path,
                                sizeof(child_path),
                                &child_attr,
                                &found_child,
                                scan_err,
                                sizeof(scan_err))) {
            if (found_child) {
                sprintf(err,
                        "deltree: %s: cannot remove directory (err=%u; first child=%s attr=0x%02X)",
                        path,
                        (unsigned int)rc,
                        child_path,
                        (unsigned int)child_attr);
            } else {
                sprintf(err,
                        "deltree: %s: cannot remove directory (err=%u; scan=empty)",
                        path,
                        (unsigned int)rc);
            }
        } else if (scan_err[0] != '\0') {
            sprintf(err,
                    "deltree: %s: cannot remove directory (err=%u; scan failed: %s)",
                    path,
                    (unsigned int)rc,
                    scan_err);
        } else {
            sprintf(err, "deltree: %s: cannot remove directory (err=%u)", path, (unsigned int)rc);
        }
        (void)err_sz;
        return 0;
    }

    return 1;
}

int fs_delete_tree(const char *root, char *err, int err_sz) {
    deltree_frame_t stack[MAX_STACK_DEPTH];
    int top;

    if (!util_copy_path(stack[0].path, sizeof(stack[0].path), root)) {
        sprintf(err, "deltree: %s: path too long", root);
        (void)err_sz;
        return 0;
    }
    top = 0;

    while (top >= 0) {
        deltree_frame_t *frame;
        char child_path[MAX_PATH_TEXT];
        u8 child_attr;
        int found_child;

        if (input_poll_abort()) {
            sprintf(err, "deltree: Aborted");
            (void)err_sz;
            return 0;
        }

        frame = &stack[top];

        if (!fs_find_first_child(frame->path,
                                 child_path,
                                 sizeof(child_path),
                                 &child_attr,
                                 &found_child,
                                 err,
                                 err_sz)) {
            return 0;
        }

        if (!found_child) {
            if (!fs_delete_dir_with_attrs(frame->path, 0, 0, err, err_sz)) {
                return 0;
            }
            top--;
            continue;
        }

        if ((child_attr & FA_DIREC) != 0u) {
            if (top + 1 >= MAX_STACK_DEPTH) {
                sprintf(err, "deltree: %s: directory depth limit exceeded", child_path);
                (void)err_sz;
                return 0;
            }
            top++;
            if (!util_copy_path(stack[top].path, sizeof(stack[top].path), child_path)) {
                sprintf(err, "deltree: %s: path too long", child_path);
                (void)err_sz;
                return 0;
            }
            continue;
        }

        if (!fs_delete_file_with_attrs(child_path, child_attr, err, err_sz)) {
            return 0;
        }
    }

    return 1;
}
