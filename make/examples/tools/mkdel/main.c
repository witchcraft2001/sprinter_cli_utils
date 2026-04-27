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
    i16 rc;

    cmd = dss_cmdline();
    path = next_arg(&cmd);
    if (path != (char *)0 && is_exe_name(path)) {
        path = next_arg(&cmd);
    }

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
