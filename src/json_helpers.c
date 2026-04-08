/*
 * json_helpers.c  —  minimal JSON path extraction + tree/flat/table output
 * No external JSON library required.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "bad.h"
#include "bad_platform.h"

/* ═══════════════════════════════════════════════════════════════
   TINY JSON NAVIGATOR
   We walk the raw JSON string without full parsing —
   good enough for dotpath extraction used in assertions.
   ═══════════════════════════════════════════════════════════════ */

static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

/* Skip one JSON value, return pointer past it */
static const char *skip_value(const char *p);

static const char *skip_string(const char *p) {
    if (*p != '"') return p;
    p++;
    while (*p && *p != '"') {
        if (*p == '\\') p++;
        p++;
    }
    if (*p == '"') p++;
    return p;
}

static const char *skip_object(const char *p) {
    if (*p != '{') return p;
    p++;
    while (*p && *p != '}') {
        p = skip_ws(p);
        if (*p == '"') p = skip_string(p);
        p = skip_ws(p);
        if (*p == ':') p++;
        p = skip_ws(p);
        p = skip_value(p);
        p = skip_ws(p);
        if (*p == ',') p++;
    }
    if (*p == '}') p++;
    return p;
}

static const char *skip_array(const char *p) {
    if (*p != '[') return p;
    p++;
    while (*p && *p != ']') {
        p = skip_ws(p);
        if (*p == ']') break;
        p = skip_value(p);
        p = skip_ws(p);
        if (*p == ',') p++;
    }
    if (*p == ']') p++;
    return p;
}

static const char *skip_value(const char *p) {
    p = skip_ws(p);
    if (!*p) return p;
    if (*p == '"') return skip_string(p);
    if (*p == '{') return skip_object(p);
    if (*p == '[') return skip_array(p);
    /* number, bool, null */
    while (*p && *p != ',' && *p != '}' && *p != ']' && !isspace((unsigned char)*p))
        p++;
    return p;
}

/* Find a key inside a JSON object starting at '{'.
   Returns pointer to the VALUE (after the colon), or NULL. */
static const char *obj_find_key(const char *p, const char *key) {
    if (!p || *p != '{') return NULL;
    p++;
    while (*p) {
        p = skip_ws(p);
        if (*p == '}') return NULL;
        if (*p != '"') return NULL;
        /* read key */
        const char *ks = p + 1;
        p = skip_string(p);
        size_t klen = (size_t)(p - 1 - ks);
        /* compare */
        int match = (strlen(key) == klen && strncmp(ks, key, klen) == 0);
        p = skip_ws(p);
        if (*p == ':') p++;
        p = skip_ws(p);
        if (match) return p;
        p = skip_value(p);
        p = skip_ws(p);
        if (*p == ',') p++;
    }
    return NULL;
}

/* Array index access */
static const char *arr_find_index(const char *p, int idx) {
    if (!p || *p != '[') return NULL;
    p++;
    int i = 0;
    while (*p) {
        p = skip_ws(p);
        if (*p == ']') return NULL;
        if (i == idx) return p;
        p = skip_value(p);
        p = skip_ws(p);
        if (*p == ',') p++;
        i++;
    }
    return NULL;
}

/* Navigate dotpath "user.id" or "items.0.name" */
static const char *navigate(const char *json, const char *dotpath) {
    const char *p = skip_ws(json);
    char seg[128];
    const char *dp = dotpath;

    while (*dp) {
        /* read next segment */
        int i = 0;
        while (*dp && *dp != '.') seg[i++] = *dp++;
        seg[i] = '\0';
        if (*dp == '.') dp++;

        p = skip_ws(p);
        if (!p) return NULL;

        if (*p == '{') {
            p = obj_find_key(p, seg);
        } else if (*p == '[') {
            int idx = atoi(seg);
            p = arr_find_index(p, idx);
        } else {
            return NULL;
        }
        if (!p) return NULL;
        p = skip_ws(p);
    }
    return p;
}

/* Extract scalar value as a newly allocated string */
char *json_get_path(const char *json, const char *dotpath) {
    if (!json || !dotpath || !dotpath[0]) {
        /* return copy of whole body */
        return BAD_STRDUP(json ? json : "");
    }
    const char *p = navigate(json, dotpath);
    if (!p) return NULL;

    p = skip_ws(p);
    const char *end = skip_value(p);

    /* strip quotes if string */
    if (*p == '"') { p++; end--; }

    size_t len = (size_t)(end - p);
    char *out = malloc(len + 1);
    strncpy(out, p, len);
    out[len] = '\0';
    return out;
}

int json_path_exists(const char *json, const char *dotpath) {
    const char *p = navigate(json, dotpath);
    return (p != NULL);
}

/* ═══════════════════════════════════════════════════════════════
   TREE FORMATTER   — user ├─ id: 123
   ═══════════════════════════════════════════════════════════════ */

static void print_indent(int depth, int last) {
    for (int i = 0; i < depth - 1; i++) printf("│  ");
    if (depth > 0) printf(last ? "└─ " : "├─ ");
}

static void fmt_value(const char *p, const char *key, int depth, int last);

static void fmt_object(const char *p, int depth) {
    if (*p != '{') return;
    p++;
    /* collect keys first for last-child detection */
    #define MAX_KEYS 128
    const char *keys[MAX_KEYS];
    const char *vals[MAX_KEYS];
    int nk = 0;

    const char *scan = p;
    while (*scan && nk < MAX_KEYS) {
        scan = skip_ws(scan);
        if (*scan == '}') break;
        if (*scan != '"') break;
        keys[nk] = scan + 1;
        scan = skip_string(scan);
        vals[nk] = skip_ws(scan);
        if (*vals[nk] == ':') { vals[nk]++; vals[nk] = skip_ws(vals[nk]); }
        scan = skip_value(vals[nk]);
        scan = skip_ws(scan);
        if (*scan == ',') scan++;
        nk++;
    }

    for (int i = 0; i < nk; i++) {
        /* extract key name */
        const char *ks = keys[i];
        const char *ke = ks;
        while (*ke && *ke != '"') ke++;
        char kbuf[128] = {0};
        strncpy(kbuf, ks, ke - ks < 127 ? (size_t)(ke - ks) : 127);
        fmt_value(vals[i], kbuf, depth, i == nk - 1);
    }
}

static void fmt_array(const char *p, const char *key, int depth) {
    if (*p != '[') return;
    p++;
    int idx = 0;
    while (*p) {
        p = skip_ws(p);
        if (*p == ']') break;
        char kbuf[32];
        snprintf(kbuf, sizeof kbuf, "%s[%d]", key ? key : "", idx);
        fmt_value(p, kbuf, depth, 0);
        p = skip_value(p);
        p = skip_ws(p);
        if (*p == ',') p++;
        idx++;
    }
}

static void fmt_value(const char *p, const char *key, int depth, int last) {
    p = skip_ws(p);
    if (*p == '{') {
        print_indent(depth, last);
        printf("%s\n", key ? key : "{}");
        fmt_object(p, depth + 1);
    } else if (*p == '[') {
        print_indent(depth, last);
        printf("%s []\n", key ? key : "");
        fmt_array(p, key, depth + 1);
    } else {
        /* scalar */
        const char *end = skip_value(p);
        size_t len = (size_t)(end - p);
        char buf[512] = {0};
        strncpy(buf, p, len < 511 ? len : 511);
        print_indent(depth, last);
        if (key) printf("%s: %s\n", key, buf);
        else     printf("%s\n", buf);
    }
}

void fmt_print_tree(const char *json, int indent) {
    if (!json) { printf("(null)\n"); return; }
    const char *p = skip_ws(json);
    (void)indent;
    if (*p == '{') {
        fmt_object(p, 0);
    } else if (*p == '[') {
        fmt_array(p, "", 0);
    } else {
        printf("%s\n", json);
    }
}

/* ═══════════════════════════════════════════════════════════════
   FLAT FORMATTER  — user.id = 123
   ═══════════════════════════════════════════════════════════════ */

static void flat_value(const char *p, const char *prefix);

static void flat_object(const char *p, const char *prefix) {
    if (*p != '{') return;
    p++;
    while (*p) {
        p = skip_ws(p);
        if (*p == '}') break;
        if (*p != '"') break;
        const char *ks = p + 1;
        p = skip_string(p);
        const char *ke = p - 1; /* before closing quote */
        char kbuf[256] = {0};
        strncpy(kbuf, ks, ke - ks < 255 ? (size_t)(ke - ks) : 255);

        char full[512] = {0};
        if (prefix && prefix[0]) snprintf(full, 511, "%s.%s", prefix, kbuf);
        else strncpy(full, kbuf, 511);

        p = skip_ws(p);
        if (*p == ':') p++;
        p = skip_ws(p);
        flat_value(p, full);
        p = skip_value(p);
        p = skip_ws(p);
        if (*p == ',') p++;
    }
}

static void flat_array(const char *p, const char *prefix) {
    if (*p != '[') return;
    p++;
    int idx = 0;
    while (*p) {
        p = skip_ws(p);
        if (*p == ']') break;
        char full[512];
        snprintf(full, 511, "%s[%d]", prefix ? prefix : "", idx);
        flat_value(p, full);
        p = skip_value(p);
        p = skip_ws(p);
        if (*p == ',') p++;
        idx++;
    }
}

static void flat_value(const char *p, const char *prefix) {
    p = skip_ws(p);
    if (*p == '{') { flat_object(p, prefix); return; }
    if (*p == '[') { flat_array(p, prefix);  return; }
    const char *end = skip_value(p);
    size_t len = (size_t)(end - p);
    char buf[512] = {0};
    strncpy(buf, p, len < 511 ? len : 511);
    printf("%s = %s\n", prefix ? prefix : "", buf);
}

void fmt_print_flat(const char *json, const char *prefix) {
    if (!json) return;
    flat_value(skip_ws(json), prefix);
}

/* ═══════════════════════════════════════════════════════════════
   TABLE FORMATTER — for arrays of objects
   ═══════════════════════════════════════════════════════════════ */

void fmt_print_table(const char *json) {
    if (!json) return;
    const char *p = skip_ws(json);
    if (*p != '[') {
        fmt_print_tree(json, 0);
        return;
    }

    /* First pass: collect column names from first object */
    const char *fp = p + 1;
    fp = skip_ws(fp);
    if (*fp != '{') { fmt_print_tree(json, 0); return; }

    char cols[32][64];
    int  ncols = 0;

    const char *scan = fp + 1;
    while (*scan && ncols < 32) {
        scan = skip_ws(scan);
        if (*scan == '}') break;
        if (*scan != '"') break;
        const char *ks = scan + 1;
        scan = skip_string(scan);
        size_t kl = (size_t)(scan - 1 - ks);
        strncpy(cols[ncols], ks, kl < 63 ? kl : 63);
        cols[ncols][kl < 63 ? kl : 63] = '\0';
        ncols++;
        scan = skip_ws(scan);
        if (*scan == ':') scan++;
        scan = skip_value(skip_ws(scan));
        scan = skip_ws(scan);
        if (*scan == ',') scan++;
    }

    /* Print header */
    for (int c = 0; c < ncols; c++) printf("%-16s", cols[c]);
    printf("\n");
    for (int c = 0; c < ncols; c++) { for(int i=0;i<15;i++) printf("-"); printf(" "); }
    printf("\n");

    /* Print rows */
    const char *row = p + 1;
    while (*row) {
        row = skip_ws(row);
        if (*row == ']') break;
        if (*row != '{') break;
        for (int c = 0; c < ncols; c++) {
            const char *v = obj_find_key(row, cols[c]);
            if (!v) { printf("%-16s", "-"); continue; }
            v = skip_ws(v);
            const char *ve = skip_value(v);
            size_t vl = (size_t)(ve - v);
            if (*v == '"') { v++; vl -= 2; }
            char vbuf[64] = {0};
            strncpy(vbuf, v, vl < 63 ? vl : 63);
            printf("%-16s", vbuf);
        }
        printf("\n");
        row = skip_value(row);
        row = skip_ws(row);
        if (*row == ',') row++;
    }
}

void fmt_print_json_pretty(const char *json) {
    if (!json) {
        printf("null\n");
        return;
    }

    int in_str = 0;
    int esc = 0;
    int indent = 0;

    for (const char *p = json; *p; p++) {
        char c = *p;

        if (in_str) {
            putchar(c);
            if (esc) {
                esc = 0;
            } else if (c == '\\') {
                esc = 1;
            } else if (c == '"') {
                in_str = 0;
            }
            continue;
        }

        if (c == '"') {
            in_str = 1;
            putchar(c);
        } else if (c == '{' || c == '[') {
            putchar(c);
            putchar('\n');
            indent++;
            for (int i = 0; i < indent; i++) printf("  ");
        } else if (c == '}' || c == ']') {
            putchar('\n');
            if (indent > 0) indent--;
            for (int i = 0; i < indent; i++) printf("  ");
            putchar(c);
        } else if (c == ',') {
            putchar(c);
            putchar('\n');
            for (int i = 0; i < indent; i++) printf("  ");
        } else if (c == ':') {
            printf(": ");
        } else if (!isspace((unsigned char)c)) {
            putchar(c);
        }
    }
    putchar('\n');
}
