#include "make.h"

void util_strip_comment(char *s) {
    char q;
    char c;

    q = 0;
    while ((c = *s) != '\0') {
        if (c == '\'' || c == '"') {
            if (q == 0) {
                q = c;
            } else if (q == c) {
                q = 0;
            }
        } else if (c == '#' && q == 0) {
            *s = '\0';
            return;
        }
        s++;
    }
}

void util_rtrim(char *s) {
    int n;

    n = (int)strlen(s);
    while (n > 0 && (unsigned char)s[n - 1] <= ' ') {
        s[n - 1] = '\0';
        n--;
    }
}

void util_ltrim(char *s) {
    char *p;
    char *d;

    p = s;
    while (*p != '\0' && (unsigned char)*p <= ' ') {
        p++;
    }
    if (p != s) {
        d = s;
        while (*p != '\0') {
            *d++ = *p++;
        }
        *d = '\0';
    }
}

void util_trim(char *s) {
    util_ltrim(s);
    util_rtrim(s);
}

int util_is_blank(const char *s) {
    while (*s != '\0') {
        if ((unsigned char)*s > ' ') {
            return 0;
        }
        s++;
    }
    return 1;
}

int util_streq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

int util_strieq(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0');
}

int util_parse_cmdline(char *buf, char *argv[MAX_ARGV]) {
    int argc;
    char *p;

    argc = 0;
    p = buf;
    while (*p != '\0' && argc < MAX_ARGV) {
        while (*p == ' ') {
            p++;
        }
        if (*p == '\0' || *p == '\r' || *p == '\n') {
            break;
        }
        argv[argc++] = p;
        while (*p != '\0' && *p != ' ' && *p != '\r' && *p != '\n') {
            p++;
        }
        if (*p != '\0') {
            *p = '\0';
            p++;
        }
    }
    return argc;
}
