#include <stdio.h>
#include <stdlib.h>
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
    char *arg;
    int code;

    cmd = dss_cmdline();
    arg = next_arg(&cmd);

    if (arg == (char *)0) {
        code = 1;
    } else {
        code = atoi(arg);
    }

    printf("mkfail: exiting with %d\n", code);
    dss_exit((unsigned char)code);
}
