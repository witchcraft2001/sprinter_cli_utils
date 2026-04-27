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

static int is_exe_name(const char *s) {
    unsigned int n;

    n = (unsigned int)strlen(s);
    if (n < 5) {
        return 0;
    }
    s += n - 4;
    return (s[0] == '.' &&
            (s[1] == 'e' || s[1] == 'E') &&
            (s[2] == 'x' || s[2] == 'X') &&
            (s[3] == 'e' || s[3] == 'E'));
}

void main(void) {
    char *cmd;
    char *path;
    char *msg;
    i16 fd;
    unsigned int len;

    cmd = dss_cmdline();
    path = next_arg(&cmd);
    if (path != (char *)0 && is_exe_name(path)) {
        path = next_arg(&cmd);
    }
    msg = next_arg(&cmd);

    if (path == (char *)0) {
        printf("mkstamp usage: mkstamp <file> [text]\n");
        dss_exit(1);
        return;
    }

    fd = dss_creat(path);
    if (fd < 0) {
        printf("mkstamp: cannot create %s\n", path);
        dss_exit(1);
        return;
    }

    if (msg == (char *)0) {
        msg = "stamp";
    }

    len = (unsigned int)strlen(msg);
    if (len != 0 && dss_write((u8)fd, msg, (u16)len) != (i16)len) {
        printf("mkstamp: cannot write %s\n", path);
        dss_close((u8)fd);
        dss_exit(1);
        return;
    }

    if (dss_write((u8)fd, "\r\n", 2) != 2) {
        printf("mkstamp: cannot finish %s\n", path);
        dss_close((u8)fd);
        dss_exit(1);
        return;
    }

    dss_close((u8)fd);
    dss_exit(0);
}
