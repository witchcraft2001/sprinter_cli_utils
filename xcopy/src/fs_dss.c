#include "xcopy.h"

typedef struct {
    char src[MAX_PATH_TEXT];
    char dst[MAX_PATH_TEXT];
} copy_frame_t;

typedef struct {
    char name[XCOPY_ENTRY_NAME_MAX];
    u8 attr;
} copy_entry_t;

const char *g_xcopy_attr_path;
u8 g_xcopy_attr_mode;
u8 g_xcopy_attr_value;
u8 g_xcopy_attr_result;
static copy_frame_t g_copy_stack[MAX_STACK_DEPTH];
static dss_find_t g_scan;
static dss_find_t g_probe;
static copy_entry_t g_dir_entries[XCOPY_MAX_DIR_ENTRIES];
static copy_entry_t g_wild_entries[XCOPY_MAX_DIR_ENTRIES];
static char g_tree_current_src[MAX_PATH_TEXT];
static char g_tree_current_dst[MAX_PATH_TEXT];
static char g_tree_pattern[MAX_PATH_TEXT];
static char g_tree_entry_name[MAX_PATH_TEXT];
static char g_tree_child_src[MAX_PATH_TEXT];
static char g_tree_child_dst[MAX_PATH_TEXT];
static char g_wild_pattern[MAX_PATH_TEXT];
static char g_wild_entry_name[MAX_PATH_TEXT];
static char g_wild_child_src[MAX_PATH_TEXT];
static char g_wild_child_dst[MAX_PATH_TEXT];
static char g_ensure_work[MAX_PATH_TEXT];
static char g_parent_path[MAX_PATH_TEXT];
static char g_single_base[MAX_PATH_TEXT];
static char g_run_src_parent[MAX_PATH_TEXT];
static char g_run_src_mask[MAX_PATH_TEXT];
static char g_run_dst_path[MAX_PATH_TEXT];

u8 xcopy_dss_attrib_call(void) __naked {
    __asm
        push    ix
        ld      hl, (_g_xcopy_attr_path)
        ld      a, (_g_xcopy_attr_mode)
        ld      b, a
        ld      a, (_g_xcopy_attr_value)
        ld      c, #0x16
        rst     #0x10
        ld      (_g_xcopy_attr_result), a
        pop     ix
        jr      c, _xcopy_attr_err
        ld      hl, #0x0000
        ret
_xcopy_attr_err:
        ld      hl, #0x0001
        ret
    __endasm;
}

int fs_getattr(const char *path, u8 *attr, u8 *err_code) {
    g_xcopy_attr_path = path;
    g_xcopy_attr_mode = 0u;
    g_xcopy_attr_value = 0u;
    if (xcopy_dss_attrib_call() != 0u) {
        if (err_code != (u8 *)0) {
            *err_code = g_xcopy_attr_result;
        }
        return 0;
    }
    *attr = g_xcopy_attr_result;
    return 1;
}

int fs_setattr(const char *path, u8 attr, u8 *err_code) {
    g_xcopy_attr_path = path;
    g_xcopy_attr_mode = 1u;
    g_xcopy_attr_value = attr;
    if (xcopy_dss_attrib_call() != 0u) {
        if (err_code != (u8 *)0) {
            *err_code = g_xcopy_attr_result;
        }
        return 0;
    }
    return 1;
}

int fs_probe_path(const char *path, u8 *attr, int *is_dir) {
    if (dss_ffirst(path, &g_probe, 0x3Fu) != 0) {
        return 0;
    }

    if (attr != (u8 *)0) {
        *attr = g_probe.attr;
    }
    if (is_dir != (int *)0) {
        *is_dir = ((g_probe.attr & FA_DIREC) != 0u);
    }
    return 1;
}

static int fs_clear_special_attrs(const char *path, char *err, int err_sz) {
    u8 attr;
    u8 new_attr;
    u8 err_code;

    if (!fs_getattr(path, &attr, &err_code)) {
        sprintf(err, "xcopy: %s: cannot read attributes (err=%u)", path, (unsigned int)err_code);
        (void)err_sz;
        return 0;
    }

    new_attr = attr & (u8)~(FA_RDONLY | FA_HIDDEN | FA_SYSTEM);
    if (new_attr == attr) {
        return 1;
    }
    if (!fs_setattr(path, new_attr, &err_code)) {
        sprintf(err, "xcopy: %s: cannot clear attributes (err=%u)", path, (unsigned int)err_code);
        (void)err_sz;
        return 0;
    }
    return 1;
}

int fs_ensure_dir_tree(const char *path, u16 *created_count, char *err, int err_sz) {
    int i;
    int start;

    *created_count = 0u;
    if (path[0] == '\0') {
        return 1;
    }
    if (!util_copy_path(g_ensure_work, sizeof(g_ensure_work), path)) {
        sprintf(err, "xcopy: %s: path too long", path);
        (void)err_sz;
        return 0;
    }
    util_normalize_path(g_ensure_work);

    if (g_ensure_work[1] == ':') {
        start = (g_ensure_work[2] == '\\') ? 3 : 2;
    } else if (g_ensure_work[0] == '\\') {
        start = 1;
    } else {
        start = 0;
    }

    for (i = start; ; i++) {
        char saved;
        u8 attr;
        int is_dir;

        saved = g_ensure_work[i];
        if (saved != '\\' && saved != '\0') {
            continue;
        }

        g_ensure_work[i] = '\0';
        if (g_ensure_work[0] != '\0' && !(g_ensure_work[1] == ':' && g_ensure_work[2] == '\0') && !util_streq(g_ensure_work, "\\")) {
            if (fs_probe_path(g_ensure_work, &attr, &is_dir)) {
                if (!is_dir) {
                    sprintf(err, "xcopy: %s: destination component is not a directory", g_ensure_work);
                    (void)err_sz;
                    return 0;
                }
            } else {
                u8 rc;

                rc = dss_mkdir(g_ensure_work);
                if (rc != 0u && !fs_probe_path(g_ensure_work, &attr, &is_dir)) {
                    sprintf(err, "xcopy: %s: cannot create directory (err=%u)", g_ensure_work, (unsigned int)rc);
                    (void)err_sz;
                    return 0;
                }
                (*created_count)++;
            }
        }

        g_ensure_work[i] = saved;
        if (saved == '\0') {
            break;
        }
    }

    return 1;
}

int fs_ensure_parent_dir(const char *path, u16 *created_count, char *err, int err_sz) {
    *created_count = 0u;
    if (!util_parent_path(path, g_parent_path, sizeof(g_parent_path))) {
        sprintf(err, "xcopy: %s: path too long", path);
        (void)err_sz;
        return 0;
    }
    if (g_parent_path[0] == '\0') {
        return 1;
    }
    return fs_ensure_dir_tree(g_parent_path, created_count, err, err_sz);
}

static int fs_should_skip_attr(const xcopy_ctx_t *ctx, u8 attr) {
    if ((attr & FA_LABEL) != 0u) {
        return 1;
    }
    if (!ctx->opts.include_hidden && (attr & (FA_HIDDEN | FA_SYSTEM)) != 0u) {
        return 1;
    }
    return 0;
}

static int fs_load_dir_entries(xcopy_ctx_t *ctx,
                               const char *pattern,
                               copy_entry_t *entries,
                               u16 *count,
                               int *raw_found,
                               char *err,
                               int err_sz) {
    *count = 0u;
    *raw_found = 0;

    if (dss_ffirst(pattern, &g_scan, 0x3Fu) != 0) {
        return 1;
    }

    *raw_found = 1;
    while (1) {
        if (input_poll_abort()) {
            ctx->aborted = 1u;
            util_set_error(err, err_sz, "xcopy: ", "aborted");
            return 0;
        }

        if (!util_is_dot_entry(g_scan.ff_name) && (g_scan.attr & FA_LABEL) == 0u) {
            if (*count >= XCOPY_MAX_DIR_ENTRIES) {
                sprintf(err, "xcopy: %s: too many directory entries", pattern);
                (void)err_sz;
                return 0;
            }
            if (!util_copy_path(entries[*count].name,
                                sizeof(entries[*count].name),
                                g_scan.ff_name)) {
                sprintf(err, "xcopy: %s: filename too long", g_scan.ff_name);
                (void)err_sz;
                return 0;
            }
            entries[*count].attr = g_scan.attr;
            (*count)++;
        }

        if (dss_fnext(&g_scan) != 0) {
            return 1;
        }
    }
}

static int fs_remove_existing_file(const char *path, char *err, int err_sz) {
    u8 rc;

    if (!fs_clear_special_attrs(path, err, err_sz)) {
        return 0;
    }
    rc = dss_delete(path);
    if (rc != 0u) {
        sprintf(err, "xcopy: %s: cannot remove existing file (err=%u)", path, (unsigned int)rc);
        (void)err_sz;
        return 0;
    }
    return 1;
}

static int fs_copy_file_exact(xcopy_ctx_t *ctx,
                              const char *src_path,
                              u8 src_attr,
                              const char *dst_path,
                              char *err,
                              int err_sz) {
    i16 src_fd;
    i16 dst_fd;
    u8 dst_attr;
    int dst_is_dir;
    int dst_exists;
    int aborted;
    u32 loaded_total;
    u32 file_bytes;
    int hit_eof;
    u16 created_count;

    if (ctx->buffer.page_count == 0u) {
        if (ctx->opts.verbose) {
            printf("Alloc buffer\r\n");
        }
        if (!buffer_init(&ctx->buffer, &ctx->total_pages, &ctx->free_pages_before)) {
            sprintf(err, "xcopy: cannot allocate paged buffer");
            (void)err_sz;
            return 0;
        }
    }

    if (util_path_is_same(src_path, dst_path)) {
        sprintf(err, "xcopy: %s: source and destination are the same", src_path);
        (void)err_sz;
        return 0;
    }

    if (input_poll_abort()) {
        ctx->aborted = 1u;
        util_set_error(err, err_sz, "xcopy: ", "aborted");
        return 0;
    }
    file_bytes = 0ul;

    dst_exists = fs_probe_path(dst_path, &dst_attr, &dst_is_dir);
    if (dst_exists && dst_is_dir) {
        sprintf(err, "xcopy: %s: destination is a directory", dst_path);
        (void)err_sz;
        return 0;
    }

    if (dst_exists && !ctx->opts.assume_yes && !ctx->overwrite_all) {
        int confirm;

        confirm = input_confirm_overwrite(dst_path, &aborted);
        if (aborted) {
            ctx->aborted = 1u;
            util_set_error(err, err_sz, "xcopy: ", "aborted");
            return 0;
        }
        if (confirm == XCOPY_CONFIRM_NO) {
            ctx->files_skipped++;
            return 1;
        }
        if (confirm == XCOPY_CONFIRM_ALL) {
            ctx->overwrite_all = 1u;
        }
    }

    if (!fs_ensure_parent_dir(dst_path, &created_count, err, err_sz)) {
        return 0;
    }
    ctx->dirs_created += (u32)created_count;

    if (dst_exists && !fs_remove_existing_file(dst_path, err, err_sz)) {
        return 0;
    }

    printf("%s -> %s\r\n", src_path, dst_path);

    src_fd = dss_open(src_path, O_RDONLY);
    if (src_fd < 0) {
        sprintf(err, "xcopy: %s: cannot open source", src_path);
        (void)err_sz;
        return 0;
    }

    dst_fd = dss_creat(dst_path);
    if (dst_fd < 0) {
        dss_close((u8)src_fd);
        sprintf(err, "xcopy: %s: cannot create destination", dst_path);
        (void)err_sz;
        return 0;
    }

    while (1) {
        if (input_poll_abort()) {
            ctx->aborted = 1u;
            util_set_error(err, err_sz, "xcopy: ", "aborted");
            dss_close((u8)src_fd);
            dss_close((u8)dst_fd);
            return 0;
        }

        if (!buffer_load_pages(&ctx->buffer, ctx, (u8)src_fd, &loaded_total, &hit_eof, err, err_sz)) {
            dss_close((u8)src_fd);
            dss_close((u8)dst_fd);
            return 0;
        }
        if (loaded_total == 0ul) {
            break;
        }
        if (!buffer_flush_pages(&ctx->buffer, ctx, (u8)dst_fd, err, err_sz)) {
            dss_close((u8)src_fd);
            dss_close((u8)dst_fd);
            return 0;
        }
        file_bytes += loaded_total;
        if (hit_eof) {
            break;
        }
    }

    (void)dss_close((u8)src_fd);
    if (dss_close((u8)dst_fd) != 0u) {
        sprintf(err, "xcopy: %s: cannot close destination", dst_path);
        (void)err_sz;
        return 0;
    }

    if (ctx->opts.preserve_attr) {
        u8 err_code;

        if (!fs_setattr(dst_path, src_attr, &err_code)) {
            sprintf(err, "xcopy: %s: cannot set attributes (err=%u)", dst_path, (unsigned int)err_code);
            (void)err_sz;
            return 0;
        }
    }

    ctx->files_copied++;
    ctx->bytes_copied += file_bytes;
    return 1;
}

static int fs_resolve_single_file_destination(xcopy_ctx_t *ctx,
                                              const char *src_path,
                                              const char *dst_arg,
                                              char *resolved,
                                              int resolved_sz,
                                              char *err,
                                              int err_sz) {
    u8 attr;
    int is_dir;
    int aborted;
    int kind;

    if (!util_get_basename(src_path, g_single_base, sizeof(g_single_base))) {
        sprintf(err, "xcopy: %s: path too long", src_path);
        (void)err_sz;
        return 0;
    }

    if (fs_probe_path(dst_arg, &attr, &is_dir)) {
        if (is_dir) {
            if (!util_join_path(resolved, resolved_sz, dst_arg, g_single_base)) {
                sprintf(err, "xcopy: %s: destination path too long", dst_arg);
                (void)err_sz;
                return 0;
            }
        } else {
            if (!util_copy_path(resolved, resolved_sz, dst_arg)) {
                sprintf(err, "xcopy: %s: destination path too long", dst_arg);
                (void)err_sz;
                return 0;
            }
        }
        return 1;
    }

    if (ctx->opts.dst_hint_dir) {
        if (!util_join_path(resolved, resolved_sz, dst_arg, g_single_base)) {
            sprintf(err, "xcopy: %s: destination path too long", dst_arg);
            (void)err_sz;
            return 0;
        }
        return 1;
    }

    kind = input_confirm_dest_kind(dst_arg, &aborted);
    if (aborted) {
        ctx->aborted = 1u;
        util_set_error(err, err_sz, "xcopy: ", "aborted");
        return 0;
    }

    if (kind == XCOPY_DEST_DIR) {
        if (!util_join_path(resolved, resolved_sz, dst_arg, g_single_base)) {
            sprintf(err, "xcopy: %s: destination path too long", dst_arg);
            (void)err_sz;
            return 0;
        }
    } else {
        if (!util_copy_path(resolved, resolved_sz, dst_arg)) {
            sprintf(err, "xcopy: %s: destination path too long", dst_arg);
            (void)err_sz;
            return 0;
        }
    }

    return 1;
}

static int fs_copy_directory_tree(xcopy_ctx_t *ctx,
                                  const char *src_root,
                                  const char *dst_root,
                                  char *err,
                                  int err_sz) {
    int pending;

    pending = 1;
    memset(g_copy_stack, 0, sizeof(g_copy_stack));
    if (!util_copy_path(g_copy_stack[0].src, sizeof(g_copy_stack[0].src), src_root) ||
        !util_copy_path(g_copy_stack[0].dst, sizeof(g_copy_stack[0].dst), dst_root)) {
        sprintf(err, "xcopy: %s: path too long", src_root);
        (void)err_sz;
        return 0;
    }

    while (pending > 0) {
        copy_frame_t *frame;
        u16 entry_count;
        int raw_found;
        u16 i;
        u16 created_count;

        if (input_poll_abort()) {
            ctx->aborted = 1u;
            util_set_error(err, err_sz, "xcopy: ", "aborted");
            return 0;
        }

        pending--;
        frame = &g_copy_stack[pending];
        if (!util_copy_path(g_tree_current_src, sizeof(g_tree_current_src), frame->src) ||
            !util_copy_path(g_tree_current_dst, sizeof(g_tree_current_dst), frame->dst)) {
            sprintf(err, "xcopy: %s: path too long", frame->src);
            (void)err_sz;
            return 0;
        }

        if (ctx->opts.verbose) {
            printf("Scanning %s\r\n", g_tree_current_src);
        }

        if (ctx->opts.copy_empty) {
            if (!fs_ensure_dir_tree(g_tree_current_dst, &created_count, err, err_sz)) {
                return 0;
            }
            ctx->dirs_created += (u32)created_count;
        }

        if (!util_join_path(g_tree_pattern, sizeof(g_tree_pattern), g_tree_current_src, "*.*")) {
            sprintf(err, "xcopy: %s: path too long", g_tree_current_src);
            (void)err_sz;
            return 0;
        }

        if (!fs_load_dir_entries(ctx, g_tree_pattern, g_dir_entries, &entry_count, &raw_found, err, err_sz)) {
            return 0;
        }

        for (i = 0u; i < entry_count; i++) {
            u8 attr;

            attr = g_dir_entries[i].attr;
            if ((attr & FA_DIREC) != 0u || fs_should_skip_attr(ctx, attr)) {
                continue;
            }

            if (!util_copy_path(g_tree_entry_name, sizeof(g_tree_entry_name), g_dir_entries[i].name)) {
                sprintf(err, "xcopy: %s: path too long", g_dir_entries[i].name);
                (void)err_sz;
                return 0;
            }
            if (!util_join_path(g_tree_child_src, sizeof(g_tree_child_src), g_tree_current_src, g_tree_entry_name) ||
                !util_join_path(g_tree_child_dst, sizeof(g_tree_child_dst), g_tree_current_dst, g_tree_entry_name)) {
                sprintf(err, "xcopy: %s\\%s: path too long", g_tree_current_src, g_tree_entry_name);
                (void)err_sz;
                return 0;
            }

            if (!fs_copy_file_exact(ctx, g_tree_child_src, attr, g_tree_child_dst, err, err_sz)) {
                return 0;
            }
        }

        for (i = 0u; i < entry_count; i++) {
            u8 attr;

            attr = g_dir_entries[i].attr;
            if ((attr & FA_DIREC) == 0u || fs_should_skip_attr(ctx, attr)) {
                continue;
            }

            if (!util_copy_path(g_tree_entry_name, sizeof(g_tree_entry_name), g_dir_entries[i].name)) {
                sprintf(err, "xcopy: %s: path too long", g_dir_entries[i].name);
                (void)err_sz;
                return 0;
            }
            if (!util_join_path(g_tree_child_src, sizeof(g_tree_child_src), g_tree_current_src, g_tree_entry_name) ||
                !util_join_path(g_tree_child_dst, sizeof(g_tree_child_dst), g_tree_current_dst, g_tree_entry_name)) {
                sprintf(err, "xcopy: %s\\%s: path too long", g_tree_current_src, g_tree_entry_name);
                (void)err_sz;
                return 0;
            }

            if (pending >= MAX_STACK_DEPTH) {
                sprintf(err, "xcopy: %s: too many pending directories", g_tree_child_src);
                (void)err_sz;
                return 0;
            }
            memset(&g_copy_stack[pending], 0, sizeof(copy_frame_t));
            if (!util_copy_path(g_copy_stack[pending].src, sizeof(g_copy_stack[pending].src), g_tree_child_src) ||
                !util_copy_path(g_copy_stack[pending].dst, sizeof(g_copy_stack[pending].dst), g_tree_child_dst)) {
                sprintf(err, "xcopy: %s: path too long", g_tree_child_src);
                (void)err_sz;
                return 0;
            }
            if (ctx->opts.verbose) {
                printf("Queue dir %s\r\n", g_tree_child_src);
            }
            pending++;
        }
    }

    return 1;
}

static int fs_copy_wildcard_root(xcopy_ctx_t *ctx,
                                 const char *src_parent,
                                 const char *mask,
                                 const char *dst_root,
                                 char *err,
                                 int err_sz) {
    int have_any;
    u16 entry_count;
    int raw_found;
    u16 i;

    if (src_parent[0] == '\0') {
        if (!util_copy_path(g_wild_pattern, sizeof(g_wild_pattern), mask)) {
            sprintf(err, "xcopy: %s: path too long", mask);
            (void)err_sz;
            return 0;
        }
    } else if (!util_join_path(g_wild_pattern, sizeof(g_wild_pattern), src_parent, mask)) {
        sprintf(err, "xcopy: %s: path too long", src_parent);
        (void)err_sz;
        return 0;
    }

    have_any = 0;
    if (!fs_load_dir_entries(ctx, g_wild_pattern, g_wild_entries, &entry_count, &raw_found, err, err_sz)) {
        return 0;
    }
    if (!raw_found) {
        sprintf(err, "xcopy: %s: source not found", g_wild_pattern);
        (void)err_sz;
        return 0;
    }

    for (i = 0u; i < entry_count; i++) {
        u8 attr;

        if (input_poll_abort()) {
            ctx->aborted = 1u;
            util_set_error(err, err_sz, "xcopy: ", "aborted");
            return 0;
        }

        attr = g_wild_entries[i].attr;
        if ((attr & FA_DIREC) != 0u || fs_should_skip_attr(ctx, attr)) {
            continue;
        }

        have_any = 1;
        if (!util_copy_path(g_wild_entry_name, sizeof(g_wild_entry_name), g_wild_entries[i].name)) {
            sprintf(err, "xcopy: %s: path too long", g_wild_entries[i].name);
            (void)err_sz;
            return 0;
        }
        if (src_parent[0] == '\0') {
            if (!util_copy_path(g_wild_child_src, sizeof(g_wild_child_src), g_wild_entry_name)) {
                sprintf(err, "xcopy: %s: path too long", g_wild_entry_name);
                (void)err_sz;
                return 0;
            }
        } else if (!util_join_path(g_wild_child_src, sizeof(g_wild_child_src), src_parent, g_wild_entry_name)) {
            sprintf(err, "xcopy: %s\\%s: path too long", src_parent, g_wild_entry_name);
            (void)err_sz;
            return 0;
        }

        if (!util_join_path(g_wild_child_dst, sizeof(g_wild_child_dst), dst_root, g_wild_entry_name)) {
            sprintf(err, "xcopy: %s\\%s: destination path too long", dst_root, g_wild_entry_name);
            (void)err_sz;
            return 0;
        }

        if (!fs_copy_file_exact(ctx, g_wild_child_src, attr, g_wild_child_dst, err, err_sz)) {
            return 0;
        }
    }

    for (i = 0u; i < entry_count; i++) {
        u8 attr;

        if (input_poll_abort()) {
            ctx->aborted = 1u;
            util_set_error(err, err_sz, "xcopy: ", "aborted");
            return 0;
        }

        attr = g_wild_entries[i].attr;
        if ((attr & FA_DIREC) == 0u || fs_should_skip_attr(ctx, attr)) {
            continue;
        }

        have_any = 1;
        if (!util_copy_path(g_wild_entry_name, sizeof(g_wild_entry_name), g_wild_entries[i].name)) {
            sprintf(err, "xcopy: %s: path too long", g_wild_entries[i].name);
            (void)err_sz;
            return 0;
        }
        if (src_parent[0] == '\0') {
            if (!util_copy_path(g_wild_child_src, sizeof(g_wild_child_src), g_wild_entry_name)) {
                sprintf(err, "xcopy: %s: path too long", g_wild_entry_name);
                (void)err_sz;
                return 0;
            }
        } else if (!util_join_path(g_wild_child_src, sizeof(g_wild_child_src), src_parent, g_wild_entry_name)) {
            sprintf(err, "xcopy: %s\\%s: path too long", src_parent, g_wild_entry_name);
            (void)err_sz;
            return 0;
        }

        if (!util_join_path(g_wild_child_dst, sizeof(g_wild_child_dst), dst_root, g_wild_entry_name)) {
            sprintf(err, "xcopy: %s\\%s: destination path too long", dst_root, g_wild_entry_name);
            (void)err_sz;
            return 0;
        }

        if (!fs_copy_directory_tree(ctx, g_wild_child_src, g_wild_child_dst, err, err_sz)) {
            return 0;
        }
    }

    if (!have_any) {
        sprintf(err, "xcopy: %s: nothing matched after filters", g_wild_pattern);
        (void)err_sz;
        return 0;
    }

    return 1;
}

static void fs_print_u32(u32 value) {
    u8 digits[10];
    u8 *bytes;
    int byte_index;
    int bit_index;
    int digit_index;
    unsigned char started;

    memset(digits, 0, sizeof(digits));
    bytes = (u8 *)&value;

    for (byte_index = 3; byte_index >= 0; byte_index--) {
        for (bit_index = 7; bit_index >= 0; bit_index--) {
            unsigned char carry;

            carry = (bytes[byte_index] & (u8)(1u << bit_index)) ? 1u : 0u;
            for (digit_index = 9; digit_index >= 0; digit_index--) {
                unsigned char digit;

                digit = (unsigned char)((digits[digit_index] << 1) + carry);
                if (digit >= 10u) {
                    digits[digit_index] = (u8)(digit - 10u);
                    carry = 1u;
                } else {
                    digits[digit_index] = (u8)digit;
                    carry = 0u;
                }
            }
        }
    }

    started = 0u;
    for (digit_index = 0; digit_index < 10; digit_index++) {
        if (digits[digit_index] != 0u || started) {
            putchar((int)('0' + digits[digit_index]));
            started = 1u;
        }
    }
    if (!started) {
        putchar('0');
    }
}

static void fs_print_stat_u32(const char *label, u32 value) {
    printf("%s", label);
    fs_print_u32(value);
    printf("\r\n");
}

int xcopy_run(xcopy_ctx_t *ctx, char *err, int err_sz) {
    u8 src_attr;
    int src_is_dir;

    ctx->start_cs = 0ul;

    if (util_has_wildcards(ctx->opts.src)) {
        u8 dst_attr;
        int dst_is_dir;

        if (!util_split_parent_mask(ctx->opts.src, g_run_src_parent, sizeof(g_run_src_parent), g_run_src_mask, sizeof(g_run_src_mask))) {
            sprintf(err, "xcopy: %s: path too long", ctx->opts.src);
            (void)err_sz;
            return 0;
        }
        if (fs_probe_path(ctx->opts.dst, &dst_attr, &dst_is_dir) && !dst_is_dir) {
            sprintf(err, "xcopy: %s: destination must be a directory", ctx->opts.dst);
            (void)err_sz;
            return 0;
        }
        return fs_copy_wildcard_root(ctx, g_run_src_parent, g_run_src_mask, ctx->opts.dst, err, err_sz);
    }

    if (ctx->opts.verbose) {
        printf("Probe %s\r\n", ctx->opts.src);
    }
    if (!fs_probe_path(ctx->opts.src, &src_attr, &src_is_dir)) {
        sprintf(err, "xcopy: %s: source not found", ctx->opts.src);
        (void)err_sz;
        return 0;
    }

    if ((src_attr & FA_DIREC) != 0u) {
        u8 dst_attr;
        int dst_is_dir;

        if (ctx->opts.verbose) {
            printf("Source is directory\r\n");
        }
        if (fs_probe_path(ctx->opts.dst, &dst_attr, &dst_is_dir) && !dst_is_dir) {
            sprintf(err, "xcopy: %s: destination is not a directory", ctx->opts.dst);
            (void)err_sz;
            return 0;
        }
        if (util_path_is_subpath(ctx->opts.dst, ctx->opts.src)) {
            sprintf(err, "xcopy: %s: destination cannot be inside source", ctx->opts.dst);
            (void)err_sz;
            return 0;
        }
        return fs_copy_directory_tree(ctx, ctx->opts.src, ctx->opts.dst, err, err_sz);
    }

    if (!fs_resolve_single_file_destination(ctx, ctx->opts.src, ctx->opts.dst, g_run_dst_path, sizeof(g_run_dst_path), err, err_sz)) {
        return 0;
    }
    return fs_copy_file_exact(ctx, ctx->opts.src, src_attr, g_run_dst_path, err, err_sz);
}

void xcopy_print_stats(const xcopy_ctx_t *ctx) {
    printf("\r\n");
    fs_print_stat_u32("Files copied : ", ctx->files_copied);
    fs_print_stat_u32("Files skipped: ", ctx->files_skipped);
    fs_print_stat_u32("Dirs created : ", ctx->dirs_created);
    fs_print_stat_u32("Bytes copied : ", ctx->bytes_copied);
}
