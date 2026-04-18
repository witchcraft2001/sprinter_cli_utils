#include <stdio.h>
#include <sprinter/dss.h>

static char *next_arg(char **pp) {
    char *p;
    char *start;

    p = *pp;
    while (*p == ' ') {
        p++;
    }
    if (*p == '\0' || *p == '\r' || *p == '\n') {
        return (char *)0;
    }

    start = p;
    while (*p != '\0' && *p != ' ' && *p != '\r' && *p != '\n') {
        p++;
    }
    if (*p != '\0') {
        *p = '\0';
        p++;
    }
    *pp = p;
    return start;
}

void main(void) {
    char *cmd;
    char *path;
    i16 rc;

    cmd = dss_cmdline();
    path = next_arg(&cmd);

    if (path == (char *)0) {
        printf("mkdel usage: mkdel <file>\n");
        dss_exit(1);
        return;
    }

    rc = dss_delete(path);
    if (rc < 0) {
        dss_exit(1);
        return;
    }

    dss_exit(0);
}
