#include "deltree.h"

typedef struct {
    char path[MAX_PATH_TEXT];
    u8 dir_attr;
    unsigned char has_dir_attr;
    unsigned char expanded;
} deltree_frame_t;

typedef struct {
    char name[ENTRY_NAME_MAX];
    u8 attr;
} deltree_entry_t;

const char *g_fs_attr_path;
u8 g_fs_attr_mode;
u8 g_fs_attr_value;
u8 g_fs_attr_result;
static deltree_frame_t g_delete_stack[MAX_STACK_DEPTH];
static dss_find_t g_scan;
static deltree_entry_t g_dir_entries[MAX_DIR_ENTRIES];
static unsigned char g_fs_progress_enabled = 1u;

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
        ld      hl, #0x0000
        ret
_fs_attr_err:
        ld      hl, #0x0001
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

    if (has_known_attr) {
        attr = known_attr;
    } else {
        if (!fs_dss_getattr(path, &attr, &err_code)) {
            sprintf(err, "deltree: %s: cannot read attributes (err=%u)", path, (unsigned int)err_code);
            (void)err_sz;
            return 0;
        }
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

void fs_set_progress_enabled(unsigned char enabled) {
    g_fs_progress_enabled = enabled;
}

static int fs_load_dir_entries(const char *dir,
                               u16 *count,
                               char *err,
                               int err_sz) {
    char pattern[MAX_PATH_TEXT];

    *count = 0u;

    if (!util_join_path(pattern, sizeof(pattern), dir, "*.*")) {
        sprintf(err, "deltree: %s: path too long", dir);
        (void)err_sz;
        return 0;
    }
    if (dss_ffirst(pattern, &g_scan, 0x3Fu) != 0) {
        return 1;
    }

    while (1) {
        if (!util_is_dot_entry(g_scan.ff_name) && (g_scan.attr & FA_LABEL) == 0u) {
            if (*count >= MAX_DIR_ENTRIES) {
                sprintf(err, "deltree: %s: too many directory entries", dir);
                (void)err_sz;
                return 0;
            }
            if (!util_copy_path(g_dir_entries[*count].name,
                                sizeof(g_dir_entries[*count].name),
                                g_scan.ff_name)) {
                sprintf(err, "deltree: %s: filename too long", g_scan.ff_name);
                (void)err_sz;
                return 0;
            }
            g_dir_entries[*count].attr = g_scan.attr;
            (*count)++;
        }

        if (dss_fnext(&g_scan) != 0) {
            return 1;
        }
    }
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

static int fs_delete_dir_with_attrs(const char *path,
                                    u8 known_attr,
                                    unsigned char has_known,
                                    u8 *remove_err,
                                    char *err,
                                    int err_sz) {
    u8 rc;

    if (remove_err != (u8 *)0) {
        *remove_err = 0u;
    }

    if (!fs_clear_special_attrs(path, known_attr, has_known, err, err_sz)) {
        return 0;
    }

    if (g_fs_progress_enabled) {
        printf("removing directory: %s\r\n", path);
    }
    rc = rmdir(path);
    if (rc != 0) {
        u8 rc2;

        if (remove_err != (u8 *)0) {
            *remove_err = rc;
        }

        if (rc == 11u) {
            return 0;
        }

        if (fs_try_rmdir_via_parent(path, &rc2)) {
            return 1;
        }

        sprintf(err, "deltree: %s: cannot remove directory (err=%u)", path, (unsigned int)rc);
        (void)err_sz;
        return 0;
    }

    return 1;
}

int fs_delete_tree_known_attr(const char *root, u8 attr, char *err, int err_sz) {
    int top;
    unsigned char abort_poll_counter;

    memset(g_delete_stack, 0, sizeof(g_delete_stack));
    if (!util_copy_path(g_delete_stack[0].path, sizeof(g_delete_stack[0].path), root)) {
        sprintf(err, "deltree: %s: path too long", root);
        (void)err_sz;
        return 0;
    }
    g_delete_stack[0].dir_attr = attr;
    g_delete_stack[0].has_dir_attr = 1u;
    g_delete_stack[0].expanded = 0u;
    top = 0;
    abort_poll_counter = 0u;

    while (top >= 0) {
        deltree_frame_t *frame;

        abort_poll_counter++;
        if (abort_poll_counter >= 8u) {
            abort_poll_counter = 0u;
            if (input_poll_abort()) {
                sprintf(err, "deltree: Aborted");
                (void)err_sz;
                return 0;
            }
        }

        frame = &g_delete_stack[top];

        if (frame->expanded) {
            u8 remove_err;

            remove_err = 0u;
            if (!fs_delete_dir_with_attrs(frame->path,
                                          frame->dir_attr,
                                          frame->has_dir_attr,
                                          &remove_err,
                                          err,
                                          err_sz)) {
                if (remove_err == 11u) {
                    frame->expanded = 0u;
                    continue;
                }
                return 0;
            }
            top--;
            continue;
        }

        {
            char current_path[MAX_PATH_TEXT];
            u16 entry_count;
            u16 i;

            if (!util_copy_path(current_path, sizeof(current_path), frame->path)) {
                sprintf(err, "deltree: %s: path too long", frame->path);
                (void)err_sz;
                return 0;
            }

            if (!fs_load_dir_entries(current_path, &entry_count, err, err_sz)) {
                return 0;
            }

            for (i = 0u; i < entry_count; i++) {
                char child_path[MAX_PATH_TEXT];
                u8 child_attr;

                child_attr = g_dir_entries[i].attr;
                if ((child_attr & FA_DIREC) != 0u) {
                    continue;
                }
                if (!util_join_path(child_path, sizeof(child_path), current_path, g_dir_entries[i].name)) {
                    sprintf(err, "deltree: %s\\%s: path too long", current_path, g_dir_entries[i].name);
                    (void)err_sz;
                    return 0;
                }
                if (!fs_delete_file_with_attrs(child_path, child_attr, err, err_sz)) {
                    return 0;
                }
            }

            frame->expanded = 1u;

            for (i = 0u; i < entry_count; i++) {
                char child_path[MAX_PATH_TEXT];
                u8 child_attr;

                child_attr = g_dir_entries[i].attr;
                if ((child_attr & FA_DIREC) == 0u) {
                    continue;
                }
                if (!util_join_path(child_path, sizeof(child_path), current_path, g_dir_entries[i].name)) {
                    sprintf(err, "deltree: %s\\%s: path too long", current_path, g_dir_entries[i].name);
                    (void)err_sz;
                    return 0;
                }
                if (top + 1 >= MAX_STACK_DEPTH) {
                    sprintf(err, "deltree: %s: directory depth limit exceeded", child_path);
                    (void)err_sz;
                    return 0;
                }
                top++;
                memset(&g_delete_stack[top], 0, sizeof(deltree_frame_t));
                if (!util_copy_path(g_delete_stack[top].path, sizeof(g_delete_stack[top].path), child_path)) {
                    sprintf(err, "deltree: %s: path too long", child_path);
                    (void)err_sz;
                    return 0;
                }
                g_delete_stack[top].dir_attr = child_attr;
                g_delete_stack[top].has_dir_attr = 1u;
                g_delete_stack[top].expanded = 0u;
            }
        }
    }

    return 1;
}

int fs_delete_tree(const char *root, char *err, int err_sz) {
    u8 attr;
    u8 err_code;
    int is_dir;

    if (!fs_probe_path(root, &attr, &is_dir, &err_code)) {
        sprintf(err, "deltree: %s: path not found", root);
        (void)err_sz;
        return 0;
    }
    if (!is_dir) {
        sprintf(err, "deltree: %s: not a directory", root);
        (void)err_sz;
        return 0;
    }
    return fs_delete_tree_known_attr(root, attr, err, err_sz);
}
