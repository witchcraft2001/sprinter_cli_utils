#include <stdio.h>
#include <string.h>
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
    char *msg;
    FILE *fp;

    cmd = dss_cmdline();
    path = next_arg(&cmd);
    msg = next_arg(&cmd);

    if (path == (char *)0) {
        printf("mkstamp usage: mkstamp <file> [text]\n");
        dss_exit(1);
        return;
    }

    fp = fopen(path, "w");
    if (fp == (FILE *)0) {
        printf("mkstamp: cannot create %s\n", path);
        dss_exit(1);
        return;
    }

    if (msg != (char *)0) {
        fputs(msg, fp);
    } else {
        fputs("stamp", fp);
    }
    fputs("\r\n", fp);
    fclose(fp);
    dss_exit(0);
}
