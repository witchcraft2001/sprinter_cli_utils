#include "xcopy.h"

#define XCOPY_BUFFER_WINDOW 0xC000u
#define XCOPY_IO_CHUNK 8192u

u8 g_xcopy_setwin3_page;
u8 g_xcopy_setwin3_result;

static u8 xcopy_setwin3_call(void) __naked {
    __asm
        push    ix
        ld      a, (_g_xcopy_setwin3_page)
        ld      b, #0
        ld      c, #0x3B
        rst     #0x10
        ld      (_g_xcopy_setwin3_result), a
        pop     ix
        jr      c, _xcopy_setwin3_err
        ld      hl, #0x0000
        ret
_xcopy_setwin3_err:
        ld      hl, #0x0001
        ret
    __endasm;
}

static int buffer_check_abort(xcopy_ctx_t *ctx, char *err, int err_sz) {
    if (!input_poll_abort()) {
        return 0;
    }

    ctx->aborted = 1u;
    util_set_error(err, err_sz, "xcopy: ", "aborted");
    return 1;
}

static int buffer_map_page(const xcopy_buffer_t *buf, u8 index, char *err, int err_sz) {
    g_xcopy_setwin3_page = buf->pages[index];
    g_xcopy_setwin3_result = 0u;
    if (xcopy_setwin3_call() != 0u) {
        sprintf(err, "xcopy: cannot map buffer page (err=%u)", (unsigned int)g_xcopy_setwin3_result);
        (void)err_sz;
        return 0;
    }
    return 1;
}

static void buffer_restore_window(xcopy_buffer_t *buf) {
    if (buf->has_saved_win3) {
        outp(PORT_WIN3, buf->saved_win3);
    }
}

int buffer_init(xcopy_buffer_t *buf, u16 *total_pages, u16 *free_pages_before) {
    u8 page;
    u8 count;

    memset(buf, 0, sizeof(xcopy_buffer_t));
    *total_pages = 0u;
    *free_pages_before = 0u;
    buf->saved_win3 = inp(PORT_WIN3);
    buf->has_saved_win3 = 1u;

    count = 0u;
    while (count < XCOPY_MAX_BUFFER_PAGES) {
        page = dss_getmem();
        if (page == 0xFFu) {
            break;
        }
        buf->pages[count++] = page;
    }
    buf->page_count = count;
    return (count > 0u);
}

void buffer_free(xcopy_buffer_t *buf) {
    u8 i;

    buffer_restore_window(buf);
    for (i = 0u; i < buf->page_count; i++) {
        dss_freemem(buf->pages[i]);
    }
    memset(buf, 0, sizeof(xcopy_buffer_t));
}

u32 buffer_capacity_bytes(const xcopy_buffer_t *buf) {
    return (u32)buf->page_count * (u32)XCOPY_PAGE_SIZE;
}

int buffer_load_pages(xcopy_buffer_t *buf,
                      xcopy_ctx_t *ctx,
                      u8 fd,
                      u32 *loaded_total,
                      int *hit_eof,
                      char *err,
                      int err_sz) {
    u8 i;

    *loaded_total = 0ul;
    *hit_eof = 0;

    for (i = 0u; i < buf->page_count; i++) {
        u16 page_used;

        if (buffer_check_abort(ctx, err, err_sz)) {
            return 0;
        }

        buf->used[i] = 0u;
        page_used = 0u;
        while (page_used < XCOPY_PAGE_SIZE) {
            i16 rc;
            u16 want;

            if (buffer_check_abort(ctx, err, err_sz)) {
                return 0;
            }

            want = (u16)(XCOPY_PAGE_SIZE - page_used);
            if (want > XCOPY_IO_CHUNK) {
                want = XCOPY_IO_CHUNK;
            }

            if (!buffer_map_page(buf, i, err, err_sz)) {
                return 0;
            }
            rc = dss_read(fd, (void *)(XCOPY_BUFFER_WINDOW + page_used), want);
            buffer_restore_window(buf);
            if (rc < 0) {
                sprintf(err, "xcopy: read: DSS error");
                (void)err_sz;
                return 0;
            }
            if (rc == 0) {
                *hit_eof = 1;
                break;
            }

            page_used = (u16)(page_used + (u16)rc);
            *loaded_total += (u32)(u16)rc;
            if ((u16)rc < want) {
                *hit_eof = 1;
                break;
            }
        }

        buf->used[i] = page_used;
        if (*hit_eof) {
            break;
        }
    }

    return 1;
}

int buffer_flush_pages(xcopy_buffer_t *buf,
                       xcopy_ctx_t *ctx,
                       u8 fd,
                       char *err,
                       int err_sz) {
    u8 i;

    for (i = 0u; i < buf->page_count; i++) {
        u16 remaining;
        u16 offset;

        remaining = buf->used[i];
        if (remaining == 0u) {
            break;
        }

        if (buffer_check_abort(ctx, err, err_sz)) {
            return 0;
        }

        offset = 0u;
        while (remaining > 0u) {
            i16 rc;
            u16 want;

            if (buffer_check_abort(ctx, err, err_sz)) {
                return 0;
            }

            want = remaining;
            if (want > XCOPY_IO_CHUNK) {
                want = XCOPY_IO_CHUNK;
            }

            if (!buffer_map_page(buf, i, err, err_sz)) {
                return 0;
            }
            rc = dss_write(fd, (const void *)(XCOPY_BUFFER_WINDOW + offset), want);
            buffer_restore_window(buf);
            if (rc <= 0) {
                sprintf(err, "xcopy: write: DSS error");
                (void)err_sz;
                return 0;
            }
            remaining = (u16)(remaining - (u16)rc);
            offset = (u16)(offset + (u16)rc);
        }
        buf->used[i] = 0u;
    }

    return 1;
}
