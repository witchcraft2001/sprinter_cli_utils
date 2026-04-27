#include "make.h"

int fs_get_mtime(const char *path, unsigned int *date, unsigned int *time) {
    ffblk found;
    i16 fd;
    int rc;

    fd = dss_open(path, O_RDONLY);
    if (fd < 0) {
        MAKE_LOG("make: mtime miss '%s'\n", path);
        return 0;
    }
    dss_close((u8)fd);

    rc = findfirst(path, &found, 0x3F);
    if (rc != 0) {
        MAKE_LOG("make: mtime miss '%s'\n", path);
        return 0;
    }

    *date = found.date;
    *time = found.time;
    MAKE_LOG("make: mtime '%s' date=%u time=%u\n", path, *date, *time);
    return 1;
}
