#include "make.h"

static u8 g_make_dt_fd;
static u16 g_make_dt_year;
static u8 g_make_dt_month;
static u8 g_make_dt_day;
static u8 g_make_dt_hour;
static u8 g_make_dt_minute;
static u8 g_make_dt_second;

static u8 make_getdt_call(void) __naked {
    __asm
        push    ix
        ld      a, (_g_make_dt_fd)
        ld      c, #0x17
        rst     #0x10
        jr      c, _make_getdt_err
        ld      a, d
        ld      (_g_make_dt_day), a
        ld      a, e
        ld      (_g_make_dt_month), a
        ld      a, h
        ld      (_g_make_dt_hour), a
        ld      a, l
        ld      (_g_make_dt_minute), a
        ld      a, b
        ld      (_g_make_dt_second), a
        ld      (_g_make_dt_year), ix
        pop     ix
        ld      hl, #0x0000
        ret
_make_getdt_err:
        pop     ix
        ld      hl, #0x0001
        ret
    __endasm;
}

int fs_get_mtime(const char *path, unsigned int *date, unsigned int *time) {
    i16 fd;
    unsigned int year;

    fd = dss_open(path, O_RDONLY);
    if (fd < 0) {
        MAKE_LOG("make: mtime miss '%s'\n", path);
        return 0;
    }

    g_make_dt_fd = (u8)fd;
    if (make_getdt_call() != 0u) {
        dss_close((u8)fd);
        MAKE_LOG("make: mtime miss '%s'\n", path);
        return 0;
    }
    dss_close((u8)fd);

    year = g_make_dt_year;
    if (year >= 1980u) {
        year -= 1980u;
    }
    *date = (unsigned int)((year << 9) | ((unsigned int)g_make_dt_month << 5) | (unsigned int)g_make_dt_day);
    *time = (unsigned int)(((unsigned int)g_make_dt_hour << 11) |
                           ((unsigned int)g_make_dt_minute << 5) |
                           ((unsigned int)g_make_dt_second >> 1));
    MAKE_LOG("make: mtime '%s' date=%u time=%u\n", path, *date, *time);
    return 1;
}
