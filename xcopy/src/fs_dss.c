#include "xcopy.h"

#define XCOPY_QUEUE_WINDOW 0xC000u
#define XCOPY_QUEUE_PREV_NONE 0xFFu

typedef struct {
    u8 page;
    u8 saved_win3;
    unsigned char has_page;
    u16 count;
    u8 used;
} path_queue_t;

const char *g_xcopy_attr_path;
u8 g_xcopy_attr_mode;
u8 g_xcopy_attr_value;
u8 g_xcopy_attr_result;
static path_queue_t g_tree_queue;
static path_queue_t g_wild_queue;
static path_queue_t g_entry_queue;
static dss_find_t g_scan;
static dss_find_t g_probe;
static char g_tree_root_src[MAX_PATH_TEXT];
static char g_tree_root_dst[MAX_PATH_TEXT];
static char g_tree_current_rel[MAX_PATH_TEXT];
static char g_tree_child_rel[MAX_PATH_TEXT];
static char g_tree_current_src[MAX_PATH_TEXT];
static char g_tree_current_dst[MAX_PATH_TEXT];
static char g_tree_pattern[MAX_PATH_TEXT];
static char g_tree_entry_name[XCOPY_ENTRY_NAME_MAX];
static char g_tree_child_src[MAX_PATH_TEXT];
static char g_tree_child_dst[MAX_PATH_TEXT];
static char g_wild_pattern[MAX_PATH_TEXT];
static char g_wild_entry_name[XCOPY_ENTRY_NAME_MAX];
static char g_wild_child_src[MAX_PATH_TEXT];
static char g_wild_child_dst[MAX_PATH_TEXT];
static char g_ensure_work[MAX_PATH_TEXT];
static char g_parent_path[MAX_PATH_TEXT];
static char g_single_base[XCOPY_ENTRY_NAME_MAX];
static char g_run_src_parent[MAX_PATH_TEXT];
static char g_run_src_mask[XCOPY_ENTRY_NAME_MAX];
static char g_run_dst_path[MAX_PATH_TEXT];

static void path_queue_map(const path_queue_t *q);
static void path_queue_restore(const path_queue_t *q);

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

static int path_queue_init(path_queue_t *q, char *err, int err_sz) {
    memset(q, 0, sizeof(path_queue_t));
    q->saved_win3 = inp(PORT_WIN3);
    q->page = dss_getmem();
    if (q->page == 0xFFu) {
        sprintf(err, "xcopy: cannot allocate directory queue page");
        (void)err_sz;
        return 0;
    }
    q->has_page = 1u;
    path_queue_map(q);
    *((u8 *)XCOPY_QUEUE_WINDOW) = XCOPY_QUEUE_PREV_NONE;
    path_queue_restore(q);
    return 1;
}

static void path_queue_free(path_queue_t *q) {
    if (q->has_page) {
        while (q->page != XCOPY_QUEUE_PREV_NONE) {
            u8 page;
            u8 prev;

            page = q->page;
            path_queue_map(q);
            prev = *((u8 *)XCOPY_QUEUE_WINDOW);
            path_queue_restore(q);
            dss_freemem(page);
            q->page = prev;
        }
        outp(PORT_WIN3, q->saved_win3);
    }
    memset(q, 0, sizeof(path_queue_t));
}

static void path_queue_map(const path_queue_t *q) {
    dss_setwin(3u, q->page);
}

static void path_queue_restore(const path_queue_t *q) {
    outp(PORT_WIN3, q->saved_win3);
}

static char *path_queue_slot(u16 index) {
    return (char *)(XCOPY_QUEUE_WINDOW + XCOPY_QUEUE_HEADER_SIZE + (u16)(index * MAX_PATH_TEXT));
}

static int path_queue_push(path_queue_t *q, const char *path, char *err, int err_sz) {
    char *slot;

    if (q->used >= XCOPY_PENDING_PAGE_ENTRIES) {
        u8 prev;

        prev = q->page;
        q->page = dss_getmem();
        if (q->page == 0xFFu) {
            q->page = prev;
            sprintf(err, "xcopy: cannot allocate directory queue page");
            (void)err_sz;
            return 0;
        }
        q->used = 0u;
        path_queue_map(q);
        *((u8 *)XCOPY_QUEUE_WINDOW) = prev;
        path_queue_restore(q);
    }

    path_queue_map(q);
    slot = path_queue_slot(q->used);
    memset(slot, 0, MAX_PATH_TEXT);
    if (!util_copy_path(slot, MAX_PATH_TEXT, path)) {
        path_queue_restore(q);
        sprintf(err, "xcopy: %s: path too long", path);
        (void)err_sz;
        return 0;
    }
    path_queue_restore(q);
    q->used++;
    q->count++;
    return 1;
}

static int path_queue_pop(path_queue_t *q, char *path, int path_sz, int *found, char *err, int err_sz) {
    *found = 0;
    if (q->count == 0u) {
        return 1;
    }

    if (q->used == 0u) {
        u8 current;
        u8 prev;

        current = q->page;
        path_queue_map(q);
        prev = *((u8 *)XCOPY_QUEUE_WINDOW);
        path_queue_restore(q);
        if (prev == XCOPY_QUEUE_PREV_NONE) {
            sprintf(err, "xcopy: directory queue is corrupt");
            (void)err_sz;
            return 0;
        }
        dss_freemem(current);
        q->page = prev;
        q->used = (u8)XCOPY_PENDING_PAGE_ENTRIES;
    }

    q->used--;
    path_queue_map(q);
    if (!util_copy_path(path, path_sz, path_queue_slot(q->used))) {
        path_queue_restore(q);
        sprintf(err, "xcopy: pending path too long");
        (void)err_sz;
        return 0;
    }
    path_queue_restore(q);
    q->count--;
    *found = 1;
    return 1;
}

static int path_queue_push_entry(path_queue_t *q, const char *name, u8 attr, char *err, int err_sz) {
    char *slot;

    if (q->used >= XCOPY_PENDING_PAGE_ENTRIES) {
        u8 prev;

        prev = q->page;
        q->page = dss_getmem();
        if (q->page == 0xFFu) {
            q->page = prev;
            sprintf(err, "xcopy: cannot allocate directory entry page");
            (void)err_sz;
            return 0;
        }
        q->used = 0u;
        path_queue_map(q);
        *((u8 *)XCOPY_QUEUE_WINDOW) = prev;
        path_queue_restore(q);
    }

    path_queue_map(q);
    slot = path_queue_slot(q->used);
    memset(slot, 0, MAX_PATH_TEXT);
    slot[0] = (char)attr;
    if (!util_copy_path(slot + 1, MAX_PATH_TEXT - 1, name)) {
        path_queue_restore(q);
        sprintf(err, "xcopy: %s: filename too long", name);
        (void)err_sz;
        return 0;
    }
    path_queue_restore(q);
    q->used++;
    q->count++;
    return 1;
}

static int path_queue_pop_entry(path_queue_t *q,
                                char *name,
                                int name_sz,
                                u8 *attr,
                                int *found,
                                char *err,
                                int err_sz) {
    char *slot;

    *found = 0;
    if (q->count == 0u) {
        return 1;
    }

    if (q->used == 0u) {
        u8 current;
        u8 prev;

        current = q->page;
        path_queue_map(q);
        prev = *((u8 *)XCOPY_QUEUE_WINDOW);
        path_queue_restore(q);
        if (prev == XCOPY_QUEUE_PREV_NONE) {
            sprintf(err, "xcopy: directory entry queue is corrupt");
            (void)err_sz;
            return 0;
        }
        dss_freemem(current);
        q->page = prev;
        q->used = (u8)XCOPY_PENDING_PAGE_ENTRIES;
    }

    q->used--;
    path_queue_map(q);
    slot = path_queue_slot(q->used);
    *attr = (u8)slot[0];
    if (!util_copy_path(name, name_sz, slot + 1)) {
        path_queue_restore(q);
        sprintf(err, "xcopy: pending filename too long");
        (void)err_sz;
        return 0;
    }
    path_queue_restore(q);
    q->count--;
    *found = 1;
    return 1;
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
    if (util_is_drive_root(path)) {
        if (attr != (u8 *)0) {
            *attr = FA_DIREC;
        }
        if (is_dir != (int *)0) {
            *is_dir = 1;
        }
        return 1;
    }

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

static int fs_path_drive(const char *path) {
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':') {
        return toupper((unsigned char)path[0]);
    }
    return 0;
}

static int fs_paths_have_different_drives(const char *left, const char *right) {
    int left_drive;
    int right_drive;

    left_drive = fs_path_drive(left);
    right_drive = fs_path_drive(right);
    return left_drive != 0 && right_drive != 0 && left_drive != right_drive;
}

static int fs_copy_file_reopen_each_drive(xcopy_ctx_t *ctx,
                                          const char *src_path,
                                          const char *dst_path,
                                          u32 *file_bytes,
                                          char *err,
                                          int err_sz) {
    i16 fd;
    u32 offset;
    int hit_eof;

    *file_bytes = 0ul;

    fd = dss_creat(dst_path);
    if (fd < 0) {
        sprintf(err, "xcopy: %s: cannot create destination", dst_path);
        (void)err_sz;
        return 0;
    }
    if (dss_close((u8)fd) != 0u) {
        sprintf(err, "xcopy: %s: cannot close destination", dst_path);
        (void)err_sz;
        return 0;
    }

    offset = 0ul;
    while (1) {
        u32 loaded_total;

        fd = dss_open(src_path, O_RDONLY);
        if (fd < 0) {
            sprintf(err, "xcopy: %s: cannot open source", src_path);
            (void)err_sz;
            return 0;
        }
        if (offset != 0ul && dss_seek((u8)fd, offset, SEEK_SET) < 0) {
            dss_close((u8)fd);
            sprintf(err, "xcopy: %s: cannot seek source", src_path);
            (void)err_sz;
            return 0;
        }
        if (!buffer_load_pages(&ctx->buffer, ctx, (u8)fd, &loaded_total, &hit_eof, err, err_sz)) {
            dss_close((u8)fd);
            return 0;
        }
        (void)dss_close((u8)fd);

        if (loaded_total == 0ul) {
            break;
        }

        fd = dss_open(dst_path, O_RDWR);
        if (fd < 0) {
            sprintf(err, "xcopy: %s: cannot open destination", dst_path);
            (void)err_sz;
            return 0;
        }
        if (dss_seek((u8)fd, offset, SEEK_SET) < 0) {
            dss_close((u8)fd);
            sprintf(err, "xcopy: %s: cannot seek destination", dst_path);
            (void)err_sz;
            return 0;
        }
        if (!buffer_flush_pages(&ctx->buffer, ctx, (u8)fd, err, err_sz)) {
            dss_close((u8)fd);
            return 0;
        }
        if (dss_close((u8)fd) != 0u) {
            sprintf(err, "xcopy: %s: cannot close destination", dst_path);
            (void)err_sz;
            return 0;
        }

        offset += loaded_total;
        *file_bytes += loaded_total;
        if (hit_eof) {
            break;
        }
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

    if (fs_paths_have_different_drives(src_path, dst_path)) {
        if (!fs_copy_file_reopen_each_drive(ctx, src_path, dst_path, &file_bytes, err, err_sz)) {
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
    int found;
    u8 attr;
    u16 created_count;

    if (!util_copy_path(g_tree_root_src, sizeof(g_tree_root_src), src_root) ||
        !util_copy_path(g_tree_root_dst, sizeof(g_tree_root_dst), dst_root)) {
        sprintf(err, "xcopy: %s: path too long", src_root);
        (void)err_sz;
        return 0;
    }

    if (!path_queue_init(&g_tree_queue, err, err_sz)) {
        return 0;
    }
    if (!path_queue_push(&g_tree_queue, "", err, err_sz)) {
        path_queue_free(&g_tree_queue);
        return 0;
    }

    while (1) {
        if (!path_queue_pop(&g_tree_queue,
                            g_tree_current_rel,
                            sizeof(g_tree_current_rel),
                            &found,
                            err,
                            err_sz)) {
            path_queue_free(&g_tree_queue);
            return 0;
        }
        if (!found) {
            break;
        }

        if (g_tree_current_rel[0] == '\0') {
            if (!util_copy_path(g_tree_current_src, sizeof(g_tree_current_src), g_tree_root_src) ||
                !util_copy_path(g_tree_current_dst, sizeof(g_tree_current_dst), g_tree_root_dst)) {
                sprintf(err, "xcopy: %s: path too long", g_tree_root_src);
                (void)err_sz;
                path_queue_free(&g_tree_queue);
                return 0;
            }
        } else if (!util_join_path(g_tree_current_src, sizeof(g_tree_current_src), g_tree_root_src, g_tree_current_rel) ||
                   !util_join_path(g_tree_current_dst, sizeof(g_tree_current_dst), g_tree_root_dst, g_tree_current_rel)) {
            sprintf(err, "xcopy: %s\\%s: path too long", g_tree_root_src, g_tree_current_rel);
            (void)err_sz;
            path_queue_free(&g_tree_queue);
            return 0;
        }

        if (input_poll_abort()) {
            ctx->aborted = 1u;
            util_set_error(err, err_sz, "xcopy: ", "aborted");
            path_queue_free(&g_tree_queue);
            return 0;
        }

        if (!util_join_path(g_tree_pattern, sizeof(g_tree_pattern), g_tree_current_src, "*.*")) {
            sprintf(err, "xcopy: %s: path too long", g_tree_current_src);
            (void)err_sz;
            path_queue_free(&g_tree_queue);
            return 0;
        }

        if (ctx->opts.verbose) {
            printf("Scanning %s\r\n", g_tree_current_src);
        }

        if (ctx->opts.copy_empty) {
            if (!fs_ensure_dir_tree(g_tree_current_dst, &created_count, err, err_sz)) {
                path_queue_free(&g_tree_queue);
                return 0;
            }
            ctx->dirs_created += (u32)created_count;
        }

        if (dss_ffirst(g_tree_pattern, &g_scan, 0x3Fu) != 0) {
            continue;
        }

        if (!path_queue_init(&g_entry_queue, err, err_sz)) {
            path_queue_free(&g_tree_queue);
            return 0;
        }

        while (1) {
            if (input_poll_abort()) {
                ctx->aborted = 1u;
                util_set_error(err, err_sz, "xcopy: ", "aborted");
                path_queue_free(&g_entry_queue);
                path_queue_free(&g_tree_queue);
                return 0;
            }

            attr = g_scan.attr;
            if (!util_copy_path(g_tree_entry_name, sizeof(g_tree_entry_name), g_scan.ff_name)) {
                sprintf(err, "xcopy: %s: filename too long", g_scan.ff_name);
                (void)err_sz;
                path_queue_free(&g_entry_queue);
                path_queue_free(&g_tree_queue);
                return 0;
            }

            if (!util_is_dot_entry(g_tree_entry_name) &&
                (attr & FA_LABEL) == 0u &&
                !fs_should_skip_attr(ctx, attr)) {
                if (!path_queue_push_entry(&g_entry_queue, g_tree_entry_name, attr, err, err_sz)) {
                    path_queue_free(&g_entry_queue);
                    path_queue_free(&g_tree_queue);
                    return 0;
                }
            }

            if (dss_fnext(&g_scan) != 0) {
                break;
            }
        }

        while (1) {
            if (!path_queue_pop_entry(&g_entry_queue,
                                      g_tree_entry_name,
                                      sizeof(g_tree_entry_name),
                                      &attr,
                                      &found,
                                      err,
                                      err_sz)) {
                path_queue_free(&g_entry_queue);
                path_queue_free(&g_tree_queue);
                return 0;
            }
            if (!found) {
                break;
            }

            if (!util_join_path(g_tree_child_src, sizeof(g_tree_child_src), g_tree_current_src, g_tree_entry_name) ||
                !util_join_path(g_tree_child_dst, sizeof(g_tree_child_dst), g_tree_current_dst, g_tree_entry_name)) {
                sprintf(err, "xcopy: %s\\%s: path too long", g_tree_current_src, g_tree_entry_name);
                (void)err_sz;
                path_queue_free(&g_entry_queue);
                path_queue_free(&g_tree_queue);
                return 0;
            }

            if ((attr & FA_DIREC) != 0u) {
                if (g_tree_current_rel[0] == '\0') {
                    if (!path_queue_push(&g_tree_queue, g_tree_entry_name, err, err_sz)) {
                        path_queue_free(&g_entry_queue);
                        path_queue_free(&g_tree_queue);
                        return 0;
                    }
                } else if (!util_join_path(g_tree_child_rel,
                                           sizeof(g_tree_child_rel),
                                           g_tree_current_rel,
                                           g_tree_entry_name)) {
                    sprintf(err, "xcopy: %s\\%s: path too long", g_tree_current_rel, g_tree_entry_name);
                    (void)err_sz;
                    path_queue_free(&g_entry_queue);
                    path_queue_free(&g_tree_queue);
                    return 0;
                } else if (!path_queue_push(&g_tree_queue, g_tree_child_rel, err, err_sz)) {
                    path_queue_free(&g_entry_queue);
                    path_queue_free(&g_tree_queue);
                    return 0;
                }
            } else if (!fs_copy_file_exact(ctx, g_tree_child_src, attr, g_tree_child_dst, err, err_sz)) {
                path_queue_free(&g_entry_queue);
                path_queue_free(&g_tree_queue);
                return 0;
            }
        }

        path_queue_free(&g_entry_queue);
    }

    path_queue_free(&g_tree_queue);
    return 1;
}

static int fs_copy_wildcard_root(xcopy_ctx_t *ctx,
                                 const char *src_parent,
                                 const char *mask,
                                 const char *dst_root,
                                 char *err,
                                 int err_sz) {
    int have_any;
    int found;
    u8 attr;

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
    if (dss_ffirst(g_wild_pattern, &g_scan, 0x3Fu) != 0) {
        sprintf(err, "xcopy: %s: source not found", g_wild_pattern);
        (void)err_sz;
        return 0;
    }

    if (!path_queue_init(&g_wild_queue, err, err_sz)) {
        return 0;
    }
    if (!path_queue_init(&g_entry_queue, err, err_sz)) {
        path_queue_free(&g_wild_queue);
        return 0;
    }

    while (1) {
        if (input_poll_abort()) {
            ctx->aborted = 1u;
            util_set_error(err, err_sz, "xcopy: ", "aborted");
            path_queue_free(&g_entry_queue);
            path_queue_free(&g_wild_queue);
            return 0;
        }

        attr = g_scan.attr;
        if (!util_copy_path(g_wild_entry_name, sizeof(g_wild_entry_name), g_scan.ff_name)) {
            sprintf(err, "xcopy: %s: filename too long", g_scan.ff_name);
            (void)err_sz;
            path_queue_free(&g_entry_queue);
            path_queue_free(&g_wild_queue);
            return 0;
        }

        if (!util_is_dot_entry(g_wild_entry_name) &&
            (attr & FA_LABEL) == 0u &&
            !fs_should_skip_attr(ctx, attr)) {
            have_any = 1;
            if (!path_queue_push_entry(&g_entry_queue, g_wild_entry_name, attr, err, err_sz)) {
                path_queue_free(&g_entry_queue);
                path_queue_free(&g_wild_queue);
                return 0;
            }
        }

        if (dss_fnext(&g_scan) != 0) {
            break;
        }
    }

    if (!have_any) {
        sprintf(err, "xcopy: %s: nothing matched after filters", g_wild_pattern);
        (void)err_sz;
        path_queue_free(&g_entry_queue);
        path_queue_free(&g_wild_queue);
        return 0;
    }

    while (1) {
        if (!path_queue_pop_entry(&g_entry_queue,
                                  g_wild_entry_name,
                                  sizeof(g_wild_entry_name),
                                  &attr,
                                  &found,
                                  err,
                                  err_sz)) {
            path_queue_free(&g_entry_queue);
            path_queue_free(&g_wild_queue);
            return 0;
        }
        if (!found) {
            break;
        }

        if (src_parent[0] == '\0') {
            if (!util_copy_path(g_wild_child_src, sizeof(g_wild_child_src), g_wild_entry_name)) {
                sprintf(err, "xcopy: %s: path too long", g_wild_entry_name);
                (void)err_sz;
                path_queue_free(&g_entry_queue);
                path_queue_free(&g_wild_queue);
                return 0;
            }
        } else if (!util_join_path(g_wild_child_src, sizeof(g_wild_child_src), src_parent, g_wild_entry_name)) {
            sprintf(err, "xcopy: %s\\%s: path too long", src_parent, g_wild_entry_name);
            (void)err_sz;
            path_queue_free(&g_entry_queue);
            path_queue_free(&g_wild_queue);
            return 0;
        }

        if (!util_join_path(g_wild_child_dst, sizeof(g_wild_child_dst), dst_root, g_wild_entry_name)) {
            sprintf(err, "xcopy: %s\\%s: destination path too long", dst_root, g_wild_entry_name);
            (void)err_sz;
            path_queue_free(&g_entry_queue);
            path_queue_free(&g_wild_queue);
            return 0;
        }

        if ((attr & FA_DIREC) != 0u) {
            if (!path_queue_push(&g_wild_queue, g_wild_entry_name, err, err_sz)) {
                path_queue_free(&g_entry_queue);
                path_queue_free(&g_wild_queue);
                return 0;
            }
        } else if (!fs_copy_file_exact(ctx, g_wild_child_src, attr, g_wild_child_dst, err, err_sz)) {
            path_queue_free(&g_entry_queue);
            path_queue_free(&g_wild_queue);
            return 0;
        }
    }

    path_queue_free(&g_entry_queue);

    while (1) {
        if (!path_queue_pop(&g_wild_queue,
                            g_wild_entry_name,
                            sizeof(g_wild_entry_name),
                            &found,
                            err,
                            err_sz)) {
            path_queue_free(&g_wild_queue);
            return 0;
        }
        if (!found) {
            break;
        }

        if (src_parent[0] == '\0') {
            if (!util_copy_path(g_wild_child_src, sizeof(g_wild_child_src), g_wild_entry_name)) {
                sprintf(err, "xcopy: %s: path too long", g_wild_entry_name);
                (void)err_sz;
                path_queue_free(&g_wild_queue);
                return 0;
            }
        } else if (!util_join_path(g_wild_child_src, sizeof(g_wild_child_src), src_parent, g_wild_entry_name)) {
            sprintf(err, "xcopy: %s\\%s: path too long", src_parent, g_wild_entry_name);
            (void)err_sz;
            path_queue_free(&g_wild_queue);
            return 0;
        }
        if (!util_join_path(g_wild_child_dst, sizeof(g_wild_child_dst), dst_root, g_wild_entry_name)) {
            sprintf(err, "xcopy: %s\\%s: destination path too long", dst_root, g_wild_entry_name);
            (void)err_sz;
            path_queue_free(&g_wild_queue);
            return 0;
        }
        if (!fs_copy_directory_tree(ctx, g_wild_child_src, g_wild_child_dst, err, err_sz)) {
            path_queue_free(&g_wild_queue);
            return 0;
        }
    }

    path_queue_free(&g_wild_queue);
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
