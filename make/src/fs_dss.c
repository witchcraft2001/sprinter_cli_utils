#include "make.h"

int fs_get_mtime(const char *path, unsigned int *date, unsigned int *time) {
    ffblk found;
    int rc;

    rc = findfirst(path, &found, 0x3F);
    if (rc != 0) {
        return 0;
    }

    *date = found.date;
    *time = found.time;
    return 1;
}
