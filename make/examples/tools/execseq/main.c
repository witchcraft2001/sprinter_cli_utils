#include <stdio.h>
#include <sprinter/dss.h>

static int exists(const char *path) {
    i16 fd;

    fd = dss_open(path, O_RDONLY);
    if (fd < 0) {
        return 0;
    }
    dss_close((u8)fd);
    return 1;
}

static int run_one(const char *cmd, const char *target) {
    i16 rc;

    printf("exec: %s\n", cmd);
    rc = dss_exec(cmd);
    printf("rc=%d exists=%d file=%s\n", rc, exists(target), target);
    return (rc == 0 && exists(target)) ? 0 : 1;
}

void main(void) {
    int failed;

    failed = 0;
    failed |= run_one("MKSTAMP.EXE seq1.txt one", "seq1.txt");
    failed |= run_one("MKSTAMP.EXE seq2.txt two", "seq2.txt");
    failed |= run_one("MKSTAMP.EXE seq3.txt three", "seq3.txt");

    if (failed) {
        printf("execseq: failed\n");
        dss_exit(1);
        return;
    }

    printf("execseq: ok\n");
    dss_exit(0);
}
