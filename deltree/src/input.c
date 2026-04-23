#include "deltree.h"

static int key_is_abort(const dss_key_t *key) {
    unsigned char scan;

    if (key->ascii == 27u) {
        return 1;
    }

    scan = (unsigned char)(key->scan & 0x7Fu);
    if ((key->modifiers & DSS_KEYMOD_CTRL) != 0u) {
        if (key->ascii == 'X' || key->ascii == 'x' ||
            key->ascii == 'Z' || key->ascii == 'z' ||
            key->ascii == 24u || key->ascii == 26u) {
            return 1;
        }
        if (scan == 0x2Au || scan == 0x2Bu) {
            return 1;
        }
    }

    return 0;
}

int input_poll_abort(void) {
    dss_key_t key;

    if (!dss_kbhit()) {
        return 0;
    }

    dss_waitkey_ex(&key);
    return key_is_abort(&key);
}

int input_confirm_delete(const char *path, int *aborted) {
    dss_key_t key;

    *aborted = 0;
    printf("%s: Delete directory and all its subdirectories? [y/N] ", path);

    while (1) {
        dss_waitkey_ex(&key);

        if (key_is_abort(&key)) {
            printf("\r\n");
            *aborted = 1;
            return 0;
        }

        if (key.ascii == 'y' || key.ascii == 'Y') {
            printf("y\r\n");
            return 1;
        }
        if (key.ascii == 'n' || key.ascii == 'N') {
            printf("n\r\n");
            return 0;
        }
        if (key.ascii == 13u || key.ascii == 10u) {
            printf("\r\n");
            return 0;
        }
    }
}
