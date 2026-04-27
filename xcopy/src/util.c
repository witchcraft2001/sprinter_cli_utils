#include "xcopy.h"

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

void util_read_cmdline_safe(char *out, int out_sz) {
    char *src;
    int i;
    int saw_bad;

    src = dss_cmdline();
    i = 0;
    saw_bad = 0;

    while (*src != '\0' && *src != '\r' && *src != '\n' && i < out_sz - 1) {
        unsigned char ch;

        ch = (unsigned char)*src;
        if (ch >= 32 && ch <= 126) {
            out[i++] = (char)ch;
        } else {
            saw_bad = 1;
            break;
        }
        src++;
    }
    out[i] = '\0';

    if (saw_bad) {
        out[0] = '\0';
    }
}

int util_copy_path(char *dst, int dst_sz, const char *src) {
    int n;

    n = (int)strlen(src);
    if (n >= dst_sz) {
        return 0;
    }
    strncpy(dst, src, (unsigned int)(dst_sz - 1));
    dst[dst_sz - 1] = '\0';
    return 1;
}

int util_join_path(char *out, int out_sz, const char *base, const char *name) {
    int base_len;
    int name_len;
    int need_sep;
    int total;

    base_len = (int)strlen(base);
    name_len = (int)strlen(name);
    need_sep = 1;
    if (base_len > 0) {
        char c;

        c = base[base_len - 1];
        if (c == '\\' || c == '/' || c == ':') {
            need_sep = 0;
        }
    }

    total = base_len + name_len + (need_sep ? 1 : 0);
    if (total >= out_sz) {
        return 0;
    }

    strcpy(out, base);
    if (need_sep) {
        out[base_len] = '\\';
        out[base_len + 1] = '\0';
    }
    strcat(out, name);
    return 1;
}

void util_normalize_path(char *path) {
    int i;
    int n;

    util_trim(path);
    for (i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/') {
            path[i] = '\\';
        }
    }

    n = (int)strlen(path);
    while (n > 0) {
        char c;

        c = path[n - 1];
        if (c != '\\') {
            break;
        }
        if (n == 3 && path[1] == ':') {
            break;
        }
        if (n == 1) {
            break;
        }
        path[n - 1] = '\0';
        n--;
    }
}

int util_is_dot_entry(const char *name) {
    if (name[0] == '.' && name[1] == '\0') {
        return 1;
    }
    if (name[0] == '.' && name[1] == '.' && name[2] == '\0') {
        return 1;
    }
    return 0;
}

int util_has_wildcards(const char *path) {
    while (*path != '\0') {
        if (*path == '*' || *path == '?') {
            return 1;
        }
        path++;
    }
    return 0;
}

int util_has_trailing_sep(const char *path) {
    int n;

    n = (int)strlen(path);
    if (n == 0) {
        return 0;
    }
    return (path[n - 1] == '\\' || path[n - 1] == '/');
}

int util_get_basename(const char *path, char *name, int name_sz) {
    const char *p;

    p = strrchr(path, '\\');
    if (p == (const char *)0) {
        p = strrchr(path, '/');
    }
    if (p == (const char *)0) {
        p = path;
    } else {
        p++;
    }
    return util_copy_path(name, name_sz, p);
}

int util_parent_path(const char *path, char *parent, int parent_sz) {
    char temp[MAX_PATH_TEXT];
    char *sep;

    if (!util_copy_path(temp, sizeof(temp), path)) {
        return 0;
    }
    util_normalize_path(temp);

    sep = strrchr(temp, '\\');
    if (sep == (char *)0) {
        parent[0] = '\0';
        return 1;
    }

    if (sep == temp + 2 && temp[1] == ':') {
        sep[1] = '\0';
    } else {
        *sep = '\0';
    }
    return util_copy_path(parent, parent_sz, temp);
}

int util_split_parent_mask(const char *path, char *parent, int parent_sz, char *mask, int mask_sz) {
    char temp[MAX_PATH_TEXT];
    char *sep;

    if (!util_copy_path(temp, sizeof(temp), path)) {
        return 0;
    }
    util_trim(temp);
    sep = strrchr(temp, '\\');
    if (sep == (char *)0) {
        parent[0] = '\0';
        return util_copy_path(mask, mask_sz, temp);
    }

    *sep = '\0';
    if (!util_copy_path(mask, mask_sz, sep + 1)) {
        return 0;
    }
    util_normalize_path(temp);
    return util_copy_path(parent, parent_sz, temp);
}

void util_set_error(char *dst, int dst_sz, const char *prefix, const char *detail) {
    int avail;

    if (dst_sz <= 0) {
        return;
    }
    dst[0] = '\0';

    strncpy(dst, prefix, (unsigned int)(dst_sz - 1));
    dst[dst_sz - 1] = '\0';

    avail = dst_sz - 1 - (int)strlen(dst);
    if (avail <= 0) {
        return;
    }
    strncat(dst, detail, (unsigned int)avail);
}

u32 util_mul_u32_u8(u32 value, u8 factor) {
    u32 out;

    out = 0ul;
    while (factor > 0u) {
        out += value;
        factor--;
    }
    return out;
}

u32 util_now_centis(void) {
    return 0ul;
}

int util_path_is_same(const char *a, const char *b) {
    char left[MAX_PATH_TEXT];
    char right[MAX_PATH_TEXT];

    if (!util_copy_path(left, sizeof(left), a) || !util_copy_path(right, sizeof(right), b)) {
        return 0;
    }
    util_normalize_path(left);
    util_normalize_path(right);
    return util_strieq(left, right);
}

int util_path_is_subpath(const char *path, const char *root) {
    char norm_path[MAX_PATH_TEXT];
    char norm_root[MAX_PATH_TEXT];
    int root_len;

    if (!util_copy_path(norm_path, sizeof(norm_path), path) ||
        !util_copy_path(norm_root, sizeof(norm_root), root)) {
        return 0;
    }

    util_normalize_path(norm_path);
    util_normalize_path(norm_root);

    root_len = (int)strlen(norm_root);
    if (root_len == 0) {
        return 0;
    }
    if (strncmp(norm_path, norm_root, (unsigned int)root_len) != 0) {
        return 0;
    }
    if (norm_path[root_len] == '\0') {
        return 1;
    }
    return norm_path[root_len] == '\\';
}
