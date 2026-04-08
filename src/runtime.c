/*
 * runtime.c  —  walk the AST, execute tests, print results
 */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include "bad.h"
#include "bad_platform.h"

#if defined(_WIN32)
#define BAD_NO_POSIX_REGEX 1
#else
#define BAD_NO_POSIX_REGEX 0
#include <regex.h>
#endif

/* ANSI colour helpers */
#define COL_RESET_RAW   "\033[0m"
#define COL_GREEN_RAW   "\033[32m"
#define COL_RED_RAW     "\033[31m"
#define COL_YELLOW_RAW  "\033[33m"
#define COL_CYAN_RAW    "\033[36m"
#define COL_BOLD_RAW    "\033[1m"
#define COL_DIM_RAW     "\033[2m"

#define LOG_ERROR 0
#define LOG_INFO  1
#define LOG_DEBUG 2

typedef struct {
    char method[16];
    char url[1024];
    char body[4096];
    int  has_body;
    int  hdr_count;
    char hdr_keys[64][128];
    char hdr_vals[64][512];
} RequestSnapshot;

typedef struct {
    int index;
    char kind[24];
    int ok;
    char detail[512];
} StepRecord;

typedef struct {
    char key[128];
    char val[512];
    int is_var;
} TemplatePair;

typedef struct {
    char name[128];
    char method[16];
    char path[512];
    int path_is_var;
    int body_count;
    TemplatePair body[64];
    int header_count;
    TemplatePair headers[64];
    int has_retry_count;
    int retry_count;
    int has_retry_delay_ms;
    int retry_delay_ms;
    int has_retry_backoff;
    int retry_backoff_mode;
    int has_retry_jitter_ms;
    int retry_jitter_ms;
    ASTNode *expects;
    int is_export;
} RequestTemplate;

typedef struct {
    char name[128];
    int pair_count;
    TemplatePair pairs[64];
} ObjectVar;

typedef struct {
    char pattern[512];
    ASTNode *stmts;
} UrlHook;

typedef struct {
    char name[128];
    long start_ms;
    long elapsed_ms;
    int running;
} NamedTimer;

static int g_verbose;
static int g_save;
static int g_flat;
static int g_table;
static int g_print_request;
static int g_print_response;
static int g_use_color;
static int g_fail_fast;
static int g_strict_runtime_errors;
static int g_save_steps;
static int g_json_view;
static int g_json_pretty;
static int g_remember_token;
static int g_show_time;
static int g_show_timestamp;
static int g_log_level;
static int g_retry_count;
static int g_retry_delay_ms;
static int g_retry_backoff_mode;
static int g_retry_jitter_ms;
static int g_base_url_locked;
static int g_timeout_locked;
static int g_stop_requested;
static int g_stop_current_test;
static int g_stopped_by_user;
static int g_runtime_depth;
static int g_record_seq;
static char g_base_url[512];
static char g_history_dir[256];
static char g_history_file[512];
static char g_history_format[16];
static char g_history_mode[16];
static char g_history_methods[128];
static char g_history_exclude_methods[128];
static int g_history_only_failed;
static int g_history_include_headers;
static int g_history_include_request_body;
static int g_history_include_response_body;
static int g_history_max_body_bytes;
static char g_source_file[512];
static char g_import_only[512];
static int  g_timeout_ms;

static RequestTemplate g_templates[256];
static int g_template_count;
static ObjectVar g_object_vars[256];
static int g_object_var_count;
static ASTNode *g_before_all;
static ASTNode *g_before_each;
static ASTNode *g_after_each;
static ASTNode *g_after_all;
static ASTNode *g_on_error;
static ASTNode *g_on_assertion_error;
static ASTNode *g_on_network_error;
static UrlHook g_before_url_hooks[64];
static int g_before_url_hook_count;
static UrlHook g_after_url_hooks[64];
static int g_after_url_hook_count;
static UrlHook g_on_url_error_hooks[64];
static int g_on_url_error_hook_count;
static int g_in_url_hook;
static int g_in_error_hook;
static int g_has_only_tests;
static int g_has_only_groups;
static int g_has_only_imports;
static char g_only_req[512];
static char g_only_import[512];
static int g_skipped_tests;
static int g_skipped_groups;
static int g_filtered_tests;
static int g_filtered_groups;

static Response g_last_resp;
static char g_last_resp_body[16384];
static int g_last_resp_status;
static int g_has_last_resp;
static RequestSnapshot g_last_req;
static int g_pass;
static int g_fail;
static int g_assertion_passes;
static int g_assertion_failures;
static int g_network_failures;
static int g_soft_runtime_errors;
static int g_zero_assert_tests;
static long g_last_resp_time_ms;
static long g_total_req_ms;
static int g_total_req_count;
static int g_arg_count;
static char g_args[32][256];

static NamedTimer g_timers[256];
static int g_timer_count;

static const char *resolve(ASTNode *n, char *tmp, size_t tmpsz);
static Response exec_send(ASTNode *node);
static int exec_expect(ASTNode *node, const Response *resp, char *detail, size_t detail_sz);
static void snapshot_last_response(const Response *r);
static void exec_runtime_stmt(ASTNode *stmt, Response *resp, int *has_resp,
                              StepRecord *steps, int *step_count, int *step_idx);
static int compare(const char *actual, const char *op, const char *expected);
static int list_contains_value(ASTNode *list, const char *actual);
static int eval_condition(ASTNode *cond);
static int parse_retry_backoff_mode(const char *mode);
static const char *retry_backoff_mode_name(int mode);
static int compute_retry_delay_ms(int base_delay, int attempt, int mode, int jitter_ms);
static long monotonic_ms(void);
static long epoch_ms(void);
static NamedTimer *find_timer(const char *name, int create);
static int timer_start(const char *name);
static int timer_stop(const char *name, long *elapsed_out);
static int timer_elapsed(const char *name, long *elapsed_out);
static ASTNode *clone_ast(const ASTNode *n);
static void clear_templates(void);
static int wildcard_match(const char *pattern, const char *text);
static const char *extract_url_path(const char *url);
static ObjectVar *find_object_var(const char *name);
static void clear_object_vars(void);
static void store_object_var(const char *name, ASTNode *object_node);
static int append_object_pair(TemplatePair *pairs, int *count, const char *key, const char *val, int is_var);
static int append_object_var_pairs(const char *name, TemplatePair *pairs, int *count);
static int add_url_hook(UrlHook *hooks, int *count, const char *pattern, ASTNode *stmts);
static int add_body_kv(const char *key, const char *val, TemplatePair *pairs, int *count, int max_count);
static int add_header_kv(const char *key, const char *val,
                         const char **hk, const char **hv, char hv_tmp[][512], int *hcount);
static int append_template_body_fields(const RequestTemplate *tpl, TemplatePair *pairs, int *count, int max_count);
static int append_ast_body_fields(ASTNode *body, TemplatePair *pairs, int *count, int max_count);
static char *build_body_json_from_fields(const TemplatePair *pairs, int count);
static char *build_template_override_body_json(const RequestTemplate *tpl, ASTNode *override_body);
static int append_body_json_field(char *buf, size_t bufsz, int *first, const char *key, const char *val);
static const char *template_value(const TemplatePair *p, char *tmp, size_t tmpsz);
static void run_hook_stmts(ASTNode *stmts, const char *label);
static void run_url_hooks(UrlHook *hooks, int count, const char *url, const char *path, const char *label);
static void trigger_error_hooks(const char *kind, const char *url);
static const char *resolve_stats_ref(const char *path, char *tmp, size_t tmpsz);
static void apply_runtime_option(const char *key, const char *val);
static void runtime_soft_errorf(const char *fmt, ...);
static int csv_contains_value_ci(const char *csv, const char *value);
static const char *last_path_sep(const char *path);
static int is_absolute_path(const char *path);
static int resolve_import_path(const char *raw, char *out, size_t outsz);
static const char *source_file_basename(void);
static int should_save_history_record(int test_failed);
static char *dup_with_body_limit(const char *s, int max_bytes, int *truncated_out);

static const char *cc(const char *raw) {
    return g_use_color ? raw : "";
}

static void log_msg(int level, const char *fmt, ...) {
    va_list ap;
    if (level > g_log_level) return;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

static int parse_log_level(const char *level) {
    if (!level) return LOG_INFO;
    if (strcmp(level, "error") == 0) return LOG_ERROR;
    if (strcmp(level, "debug") == 0) return LOG_DEBUG;
    return LOG_INFO;
}

static int parse_retry_backoff_mode(const char *mode) {
    if (!mode || !mode[0]) return 0;
    if (BAD_STRCASECMP(mode, "linear") == 0) return 1;
    if (BAD_STRCASECMP(mode, "exponential") == 0) return 2;
    if (BAD_STRCASECMP(mode, "fixed") == 0 || BAD_STRCASECMP(mode, "none") == 0) return 0;
    return 0;
}

static const char *retry_backoff_mode_name(int mode) {
    if (mode == 1) return "linear";
    if (mode == 2) return "exponential";
    return "fixed";
}

static int compute_retry_delay_ms(int base_delay, int attempt, int mode, int jitter_ms) {
    long delay = base_delay;
    if (delay < 0) delay = 0;

    if (mode == 1) {
        delay = (long)base_delay * (long)attempt;
    } else if (mode == 2) {
        long factor = 1;
        for (int i = 1; i < attempt; i++) {
            if (factor > 1024) break;
            factor *= 2;
        }
        delay = (long)base_delay * factor;
    }

    if (delay < 0) delay = 0;
    if (delay > 600000) delay = 600000;

    if (jitter_ms > 0) {
        delay += rand() % (jitter_ms + 1);
    }

    if (delay > 600000) delay = 600000;
    return (int)delay;
}

static void sanitize_name(char *dst, size_t dstsz, const char *src) {
    size_t j = 0;
    if (!dst || dstsz == 0) return;
    for (size_t i = 0; src && src[i] && j + 1 < dstsz; i++) {
        unsigned char c = (unsigned char)src[i];
        if (isalnum(c) || c == '-' || c == '_') dst[j++] = (char)c;
        else if (c == ' ' || c == '/' || c == '\\' || c == '.') dst[j++] = '_';
    }
    if (j == 0) dst[j++] = 'x';
    dst[j] = '\0';
}

static char *json_escape(const char *s) {
    size_t n = 0;
    char *out;
    char *w;
    if (!s) return BAD_STRDUP("");

    for (const char *p = s; *p; p++) {
        if (*p == '"' || *p == '\\' || *p == '\n' || *p == '\r' || *p == '\t') n += 2;
        else n += 1;
    }

    out = malloc(n + 1);
    if (!out) return NULL;
    w = out;

    for (const char *p = s; *p; p++) {
        if (*p == '"') { *w++ = '\\'; *w++ = '"'; }
        else if (*p == '\\') { *w++ = '\\'; *w++ = '\\'; }
        else if (*p == '\n') { *w++ = '\\'; *w++ = 'n'; }
        else if (*p == '\r') { *w++ = '\\'; *w++ = 'r'; }
        else if (*p == '\t') { *w++ = '\\'; *w++ = 't'; }
        else *w++ = *p;
    }
    *w = '\0';
    return out;
}

static int ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) return S_ISDIR(st.st_mode) ? 0 : -1;
    if (bad_mkdir(path) == 0) return 0;
    if (errno == EEXIST) return 0;
    return -1;
}

static int ensure_parent_dir(const char *path) {
    char tmp[1024];
    size_t n;
    if (!path || !*path) return 0;
    n = strlen(path);
    if (n >= sizeof(tmp)) return -1;
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    char *slash = strrchr(tmp, '/');
#ifdef _WIN32
    char *bslash = strrchr(tmp, '\\');
    if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
    if (!slash) return 0;
    *slash = '\0';
    if (!tmp[0]) return 0;

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char sep = *p;
            *p = '\0';
            if (bad_mkdir(tmp) != 0 && errno != EEXIST) return -1;
            *p = sep;
        }
    }
    if (bad_mkdir(tmp) != 0 && errno != EEXIST) return -1;
    return 0;
}

static int has_json_shape(const char *s) {
    if (!s) return 0;
    while (*s && isspace((unsigned char)*s)) s++;
    return *s == '{' || *s == '[' || *s == '"' || *s == '-' ||
           isdigit((unsigned char)*s) || *s == 't' || *s == 'f' || *s == 'n';
}

static void now_iso8601(char *dst, size_t dstsz) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    if (!tm) {
        snprintf(dst, dstsz, "1970-01-01T00:00:00Z");
        return;
    }
    strftime(dst, dstsz, "%Y-%m-%dT%H:%M:%S%z", tm);
}

static long monotonic_ms(void) {
    return bad_monotonic_ms();
}

static long epoch_ms(void) {
    return bad_epoch_ms();
}

static NamedTimer *find_timer(const char *name, int create) {
    if (!name || !name[0]) return NULL;
    for (int i = 0; i < g_timer_count; i++) {
        if (strcmp(g_timers[i].name, name) == 0) return &g_timers[i];
    }
    if (!create) return NULL;
    if (g_timer_count >= (int)(sizeof(g_timers) / sizeof(g_timers[0]))) {
        fprintf(stderr, "[bad] timer store full\n");
        return NULL;
    }

    NamedTimer *t = &g_timers[g_timer_count++];
    memset(t, 0, sizeof(*t));
    strncpy(t->name, name, sizeof(t->name) - 1);
    t->name[sizeof(t->name) - 1] = '\0';
    return t;
}

static void publish_timer_var(const char *name, long elapsed_ms) {
    char key[160];
    char val[64];
    snprintf(key, sizeof(key), "%s_ms", name);
    snprintf(val, sizeof(val), "%ld", elapsed_ms);
    var_set(key, val);
}

static int timer_start(const char *name) {
    NamedTimer *t = find_timer(name, 1);
    if (!t) return -1;
    t->start_ms = monotonic_ms();
    t->elapsed_ms = 0;
    t->running = 1;
    return 0;
}

static int timer_stop(const char *name, long *elapsed_out) {
    NamedTimer *t = find_timer(name, 0);
    if (!t || !t->running) return -1;

    long now = monotonic_ms();
    t->elapsed_ms = now - t->start_ms;
    if (t->elapsed_ms < 0) t->elapsed_ms = 0;
    t->running = 0;
    publish_timer_var(name, t->elapsed_ms);

    if (elapsed_out) *elapsed_out = t->elapsed_ms;
    return 0;
}

static int timer_elapsed(const char *name, long *elapsed_out) {
    NamedTimer *t = find_timer(name, 0);
    if (!t) return -1;
    if (t->running) {
        long now = monotonic_ms();
        long ms = now - t->start_ms;
        if (ms < 0) ms = 0;
        if (elapsed_out) *elapsed_out = ms;
        return 0;
    }
    if (elapsed_out) *elapsed_out = t->elapsed_ms;
    return 0;
}

static void step_add(StepRecord *steps, int *count, int index,
                     const char *kind, int ok, const char *detail) {
    if (!steps || !count || *count >= 512) return;
    StepRecord *s = &steps[*count];
    s->index = index;
    s->ok = ok;
    strncpy(s->kind, kind ? kind : "step", sizeof(s->kind) - 1);
    s->kind[sizeof(s->kind) - 1] = '\0';
    strncpy(s->detail, detail ? detail : "", sizeof(s->detail) - 1);
    s->detail[sizeof(s->detail) - 1] = '\0';
    (*count)++;
}

static int parse_bool_like(const char *v) {
    if (!v) return 0;
    return strcmp(v, "1") == 0 ||
           strcmp(v, "true") == 0 ||
           strcmp(v, "yes") == 0 ||
           strcmp(v, "on") == 0;
}

static int csv_contains_value_ci(const char *csv, const char *value) {
    if (!csv || !csv[0] || !value || !value[0]) return 0;

    char buf[256];
    strncpy(buf, csv, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tok = strtok(buf, ",");
    while (tok) {
        while (*tok == ' ' || *tok == '\t') tok++;
        char *end = tok + strlen(tok) - 1;
        while (end >= tok && (*end == ' ' || *end == '\t')) {
            *end = '\0';
            end--;
        }

        if (BAD_STRCASECMP(tok, value) == 0) return 1;
        tok = strtok(NULL, ",");
    }
    return 0;
}

static const char *last_path_sep(const char *path) {
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *bslash = strrchr(path, '\\');
    if (!slash || (bslash && bslash > slash)) return bslash;
#endif
    return slash;
}

static int is_absolute_path(const char *path) {
    if (!path || !path[0]) return 0;
    if (path[0] == '/') return 1;
#ifdef _WIN32
    if ((isalpha((unsigned char)path[0]) && path[1] == ':' &&
         (path[2] == '/' || path[2] == '\\')) ||
        (path[0] == '\\' && path[1] == '\\')) {
        return 1;
    }
#endif
    return 0;
}

static int resolve_import_path(const char *raw, char *out, size_t outsz) {
    if (!raw || !raw[0] || !out || outsz == 0) return 0;

    if (is_absolute_path(raw)) {
        strncpy(out, raw, outsz - 1);
        out[outsz - 1] = '\0';
        return 1;
    }

    if (!g_source_file[0]) {
        strncpy(out, raw, outsz - 1);
        out[outsz - 1] = '\0';
        return 1;
    }

    const char *sep = last_path_sep(g_source_file);
    if (!sep) {
        strncpy(out, raw, outsz - 1);
        out[outsz - 1] = '\0';
        return 1;
    }

    size_t dir_len = (size_t)(sep - g_source_file);
    if (dir_len + 1 + strlen(raw) + 1 > outsz) return 0;

    memcpy(out, g_source_file, dir_len);
    out[dir_len] = '/';
    strcpy(out + dir_len + 1, raw);
    return 1;
}

static const char *source_file_basename(void) {
    const char *base = last_path_sep(g_source_file);
    return base ? base + 1 : g_source_file;
}

static char *dup_with_body_limit(const char *s, int max_bytes, int *truncated_out) {
    size_t len;
    size_t out_len;
    char *out;

    if (truncated_out) *truncated_out = 0;
    if (!s) return BAD_STRDUP("");

    len = strlen(s);
    out_len = len;
    if (max_bytes > 0 && len > (size_t)max_bytes) {
        out_len = (size_t)max_bytes;
        if (truncated_out) *truncated_out = 1;
    }

    out = malloc(out_len + 1);
    if (!out) return NULL;
    memcpy(out, s, out_len);
    out[out_len] = '\0';
    return out;
}

static int should_save_history_record(int test_failed) {
    if (!g_save) return 0;

    if (BAD_STRCASECMP(g_history_mode, "off") == 0 ||
        BAD_STRCASECMP(g_history_mode, "none") == 0) {
        return 0;
    }

    if (g_history_only_failed && !test_failed) return 0;

    if (g_history_methods[0] && !csv_contains_value_ci(g_history_methods, g_last_req.method)) {
        return 0;
    }

    if (g_history_exclude_methods[0] && csv_contains_value_ci(g_history_exclude_methods, g_last_req.method)) {
        return 0;
    }

    return 1;
}

static void runtime_soft_errorf(const char *fmt, ...) {
    va_list ap;
    g_soft_runtime_errors++;
    fprintf(stderr, "[bad] ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

static void apply_runtime_option(const char *key, const char *val) {
    if (!key || !key[0]) return;
    if (!val) val = "";

    if (strcmp(key, "save_history") == 0) g_save = parse_bool_like(val);
    else if (strcmp(key, "save_steps") == 0) g_save_steps = parse_bool_like(val);
    else if (strcmp(key, "print_request") == 0) g_print_request = parse_bool_like(val);
    else if (strcmp(key, "print_response") == 0) g_print_response = parse_bool_like(val);
    else if (strcmp(key, "remember_token") == 0) g_remember_token = parse_bool_like(val);
    else if (strcmp(key, "show_time") == 0) g_show_time = parse_bool_like(val);
    else if (strcmp(key, "show_timestamp") == 0) g_show_timestamp = parse_bool_like(val);
    else if (strcmp(key, "json_view") == 0) g_json_view = parse_bool_like(val);
    else if (strcmp(key, "json_pretty") == 0) g_json_pretty = parse_bool_like(val);
    else if (strcmp(key, "flat") == 0) g_flat = parse_bool_like(val);
    else if (strcmp(key, "table") == 0) g_table = parse_bool_like(val);
    else if (strcmp(key, "fail_fast") == 0) g_fail_fast = parse_bool_like(val);
    else if (strcmp(key, "strict_runtime_errors") == 0 || strcmp(key, "strict_runtime") == 0)
        g_strict_runtime_errors = parse_bool_like(val);
    else if (strcmp(key, "use_color") == 0) g_use_color = parse_bool_like(val);
    else if (strcmp(key, "history_dir") == 0) {
        strncpy(g_history_dir, val, sizeof(g_history_dir) - 1);
        g_history_dir[sizeof(g_history_dir) - 1] = '\0';
    } else if (strcmp(key, "history_file") == 0) {
        strncpy(g_history_file, val, sizeof(g_history_file) - 1);
        g_history_file[sizeof(g_history_file) - 1] = '\0';
    } else if (strcmp(key, "history_mode") == 0 || strcmp(key, "save_mode") == 0) {
        strncpy(g_history_mode, val, sizeof(g_history_mode) - 1);
        g_history_mode[sizeof(g_history_mode) - 1] = '\0';
    } else if (strcmp(key, "history_methods") == 0 || strcmp(key, "save_methods") == 0) {
        strncpy(g_history_methods, val, sizeof(g_history_methods) - 1);
        g_history_methods[sizeof(g_history_methods) - 1] = '\0';
    } else if (strcmp(key, "history_exclude_methods") == 0 || strcmp(key, "save_exclude_methods") == 0) {
        strncpy(g_history_exclude_methods, val, sizeof(g_history_exclude_methods) - 1);
        g_history_exclude_methods[sizeof(g_history_exclude_methods) - 1] = '\0';
    } else if (strcmp(key, "history_only_failed") == 0 || strcmp(key, "save_only_failed") == 0) {
        g_history_only_failed = parse_bool_like(val);
    } else if (strcmp(key, "history_include_headers") == 0 || strcmp(key, "save_headers") == 0) {
        g_history_include_headers = parse_bool_like(val);
    } else if (strcmp(key, "history_include_request_body") == 0 || strcmp(key, "save_request_body") == 0) {
        g_history_include_request_body = parse_bool_like(val);
    } else if (strcmp(key, "history_include_response_body") == 0 || strcmp(key, "save_response_body") == 0) {
        g_history_include_response_body = parse_bool_like(val);
    } else if (strcmp(key, "history_max_body_bytes") == 0 || strcmp(key, "save_max_body_bytes") == 0) {
        g_history_max_body_bytes = atoi(val);
        if (g_history_max_body_bytes < 0) g_history_max_body_bytes = 0;
    } else if (strcmp(key, "history_format") == 0) {
        strncpy(g_history_format, val, sizeof(g_history_format) - 1);
        g_history_format[sizeof(g_history_format) - 1] = '\0';
    } else if (strcmp(key, "only_req") == 0) {
        strncpy(g_only_req, val, sizeof(g_only_req) - 1);
        g_only_req[sizeof(g_only_req) - 1] = '\0';
    } else if (strcmp(key, "only_import") == 0) {
        strncpy(g_only_import, val, sizeof(g_only_import) - 1);
        g_only_import[sizeof(g_only_import) - 1] = '\0';
    } else if (strcmp(key, "log_level") == 0) {
        g_log_level = parse_log_level(val);
    } else if (strcmp(key, "retry_count") == 0) {
        g_retry_count = atoi(val);
        if (g_retry_count < 0) g_retry_count = 0;
    } else if (strcmp(key, "retry_delay_ms") == 0) {
        g_retry_delay_ms = atoi(val);
        if (g_retry_delay_ms < 0) g_retry_delay_ms = 0;
    } else if (strcmp(key, "retry_backoff") == 0) {
        g_retry_backoff_mode = parse_retry_backoff_mode(val);
    } else if (strcmp(key, "retry_jitter_ms") == 0) {
        g_retry_jitter_ms = atoi(val);
        if (g_retry_jitter_ms < 0) g_retry_jitter_ms = 0;
    } else if (strcmp(key, "base_url") == 0 && !g_base_url_locked) {
        strncpy(g_base_url, val, sizeof(g_base_url) - 1);
        g_base_url[sizeof(g_base_url) - 1] = '\0';
    } else if (strcmp(key, "timeout") == 0 && !g_timeout_locked) {
        g_timeout_ms = atoi(val);
    }
}

static const char *resolve_stats_ref(const char *path, char *tmp, size_t tmpsz) {
    long avg_time_ms = (g_total_req_count > 0) ? (g_total_req_ms / g_total_req_count) : 0;
    long last_time_ms = g_has_last_resp ? g_last_resp_time_ms : -1;
    int last_status = g_has_last_resp ? g_last_resp_status : -1;
    int req_success = g_total_req_count - g_network_failures;
    if (req_success < 0) req_success = 0;
    int assertion_total = g_assertion_passes + g_assertion_failures;

    if (!path || !path[0]) {
        snprintf(tmp, tmpsz,
                 "{\"requests\":{\"total\":%d,\"successful\":%d,\"network_failures\":%d,\"last_status\":%d,\"last_time_ms\":%ld,\"avg_time_ms\":%ld,\"total_time_ms\":%ld},"
                 "\"assertions\":{\"passed\":%d,\"failed\":%d,\"total\":%d,\"current_test_passed\":%d,\"current_test_failed\":%d},"
                 "\"runtime\":{\"soft_errors\":%d,\"zero_assert_tests\":%d,\"skipped_tests\":%d,\"skipped_groups\":%d,\"filtered_tests\":%d,\"filtered_groups\":%d,\"strict_runtime_errors\":%s},"
                 "\"timers\":{\"count\":%d}}",
                 g_total_req_count, req_success, g_network_failures, last_status, last_time_ms, avg_time_ms, g_total_req_ms,
                 g_assertion_passes, g_assertion_failures, assertion_total, g_pass, g_fail,
                 g_soft_runtime_errors, g_zero_assert_tests, g_skipped_tests, g_skipped_groups,
                 g_filtered_tests, g_filtered_groups, g_strict_runtime_errors ? "true" : "false",
                 g_timer_count);
        return tmp;
    }

    if (strcmp(path, "requests.total") == 0) {
        snprintf(tmp, tmpsz, "%d", g_total_req_count);
        return tmp;
    }
    if (strcmp(path, "requests.successful") == 0) {
        snprintf(tmp, tmpsz, "%d", req_success);
        return tmp;
    }
    if (strcmp(path, "requests.failed") == 0 || strcmp(path, "requests.network_failures") == 0) {
        snprintf(tmp, tmpsz, "%d", g_network_failures);
        return tmp;
    }
    if (strcmp(path, "requests.last_status") == 0 || strcmp(path, "last.status") == 0) {
        snprintf(tmp, tmpsz, "%d", last_status);
        return tmp;
    }
    if (strcmp(path, "requests.last_time_ms") == 0 || strcmp(path, "last.time_ms") == 0) {
        snprintf(tmp, tmpsz, "%ld", last_time_ms);
        return tmp;
    }
    if (strcmp(path, "requests.avg_time_ms") == 0) {
        snprintf(tmp, tmpsz, "%ld", avg_time_ms);
        return tmp;
    }
    if (strcmp(path, "requests.total_time_ms") == 0) {
        snprintf(tmp, tmpsz, "%ld", g_total_req_ms);
        return tmp;
    }

    if (strcmp(path, "assertions.passed") == 0) {
        snprintf(tmp, tmpsz, "%d", g_assertion_passes);
        return tmp;
    }
    if (strcmp(path, "assertions.failed") == 0) {
        snprintf(tmp, tmpsz, "%d", g_assertion_failures);
        return tmp;
    }
    if (strcmp(path, "assertions.total") == 0) {
        snprintf(tmp, tmpsz, "%d", assertion_total);
        return tmp;
    }
    if (strcmp(path, "assertions.current_test_passed") == 0) {
        snprintf(tmp, tmpsz, "%d", g_pass);
        return tmp;
    }
    if (strcmp(path, "assertions.current_test_failed") == 0) {
        snprintf(tmp, tmpsz, "%d", g_fail);
        return tmp;
    }

    if (strcmp(path, "runtime.soft_errors") == 0) {
        snprintf(tmp, tmpsz, "%d", g_soft_runtime_errors);
        return tmp;
    }
    if (strcmp(path, "runtime.zero_assert_tests") == 0) {
        snprintf(tmp, tmpsz, "%d", g_zero_assert_tests);
        return tmp;
    }
    if (strcmp(path, "runtime.skipped_tests") == 0) {
        snprintf(tmp, tmpsz, "%d", g_skipped_tests);
        return tmp;
    }
    if (strcmp(path, "runtime.skipped_groups") == 0) {
        snprintf(tmp, tmpsz, "%d", g_skipped_groups);
        return tmp;
    }
    if (strcmp(path, "runtime.filtered_tests") == 0) {
        snprintf(tmp, tmpsz, "%d", g_filtered_tests);
        return tmp;
    }
    if (strcmp(path, "runtime.filtered_groups") == 0) {
        snprintf(tmp, tmpsz, "%d", g_filtered_groups);
        return tmp;
    }
    if (strcmp(path, "runtime.strict_runtime_errors") == 0) {
        snprintf(tmp, tmpsz, "%s", g_strict_runtime_errors ? "true" : "false");
        return tmp;
    }

    if (strcmp(path, "timers.count") == 0) {
        snprintf(tmp, tmpsz, "%d", g_timer_count);
        return tmp;
    }

    runtime_soft_errorf("unknown stats selector 'stats.%s'", path);
    return "";
}

static ASTNode *clone_ast(const ASTNode *n) {
    if (!n) return NULL;

    ASTNode *c = calloc(1, sizeof(*c));
    if (!c) {
        fprintf(stderr, "[bad] OOM cloning AST node\n");
        return NULL;
    }

    *c = *n;
    c->left = clone_ast(n->left);
    c->right = clone_ast(n->right);
    c->body = clone_ast(n->body);
    c->headers = clone_ast(n->headers);
    c->stmts = clone_ast(n->stmts);
    c->alt = clone_ast(n->alt);
    c->extra = clone_ast(n->extra);
    c->retry = clone_ast(n->retry);
    c->retry_delay = clone_ast(n->retry_delay);
    c->retry_backoff = clone_ast(n->retry_backoff);
    c->retry_jitter = clone_ast(n->retry_jitter);
    return c;
}

static void clear_templates(void) {
    for (int i = 0; i < g_template_count; i++) {
        ast_free(g_templates[i].expects);
        g_templates[i].expects = NULL;
    }
    g_template_count = 0;
}

static int wildcard_match(const char *pattern, const char *text) {
    if (!pattern || !text) return 0;
    if (*pattern == '\0') return *text == '\0';

    if (*pattern == '*') {
        pattern++;
        if (*pattern == '\0') return 1;
        while (*text) {
            if (wildcard_match(pattern, text)) return 1;
            text++;
        }
        return wildcard_match(pattern, text);
    }

    if (*pattern == *text) {
        return wildcard_match(pattern + 1, text + 1);
    }
    return 0;
}

static const char *extract_url_path(const char *url) {
    if (!url || !url[0]) return "/";
    const char *scheme = strstr(url, "://");
    if (!scheme) return url;
    const char *path = strchr(scheme + 3, '/');
    return path ? path : "/";
}

static ObjectVar *find_object_var(const char *name) {
    if (!name || !name[0]) return NULL;
    for (int i = 0; i < g_object_var_count; i++) {
        if (strcmp(g_object_vars[i].name, name) == 0) {
            return &g_object_vars[i];
        }
    }
    return NULL;
}

static void clear_object_vars(void) {
    g_object_var_count = 0;
}

static int append_object_pair(TemplatePair *pairs, int *count, const char *key, const char *val, int is_var) {
    if (!pairs || !count || *count >= 64 || !key || !key[0]) return -1;

    TemplatePair *p = &pairs[*count];
    strncpy(p->key, key, sizeof(p->key) - 1);
    p->key[sizeof(p->key) - 1] = '\0';
    strncpy(p->val, val ? val : "", sizeof(p->val) - 1);
    p->val[sizeof(p->val) - 1] = '\0';
    p->is_var = is_var;
    (*count)++;
    return 0;
}

static int append_object_var_pairs(const char *name, TemplatePair *pairs, int *count) {
    ObjectVar *ov = find_object_var(name);
    if (!ov) {
        runtime_soft_errorf("object variable '%s' not found", name ? name : "");
        return -1;
    }

    for (int i = 0; i < ov->pair_count; i++) {
        if (append_object_pair(pairs, count, ov->pairs[i].key, ov->pairs[i].val, 0) != 0) {
            fprintf(stderr, "[bad] object variable '%s' exceeded capacity\n", name);
            return -1;
        }
    }
    return 0;
}

static int add_url_hook(UrlHook *hooks, int *count, const char *pattern, ASTNode *stmts) {
    if (!hooks || !count || !pattern || !pattern[0]) return -1;
    if (*count >= 64) {
        fprintf(stderr, "[bad] too many URL hooks (max 64)\n");
        return -1;
    }

    UrlHook *h = &hooks[*count];
    strncpy(h->pattern, pattern, sizeof(h->pattern) - 1);
    h->pattern[sizeof(h->pattern) - 1] = '\0';
    h->stmts = stmts;
    (*count)++;
    return 0;
}

static int add_body_kv(const char *key, const char *val, TemplatePair *pairs, int *count, int max_count) {
    if (!key || !key[0] || !pairs || !count || max_count <= 0) return -1;

    for (int i = 0; i < *count; i++) {
        if (strcmp(pairs[i].key, key) == 0) {
            strncpy(pairs[i].val, val ? val : "", sizeof(pairs[i].val) - 1);
            pairs[i].val[sizeof(pairs[i].val) - 1] = '\0';
            pairs[i].is_var = 0;
            return 0;
        }
    }

    if (*count >= max_count) return -1;
    TemplatePair *p = &pairs[*count];
    memset(p, 0, sizeof(*p));
    strncpy(p->key, key, sizeof(p->key) - 1);
    p->key[sizeof(p->key) - 1] = '\0';
    strncpy(p->val, val ? val : "", sizeof(p->val) - 1);
    p->val[sizeof(p->val) - 1] = '\0';
    p->is_var = 0;
    (*count)++;
    return 0;
}

static int add_header_kv(const char *key, const char *val,
                         const char **hk, const char **hv, char hv_tmp[][512], int *hcount) {
    if (!key || !val || !hk || !hv || !hv_tmp || !hcount) return -1;

    for (int i = 0; i < *hcount; i++) {
        if (BAD_STRCASECMP(hk[i], key) == 0) {
            hk[i] = key;
            snprintf(hv_tmp[i], sizeof(hv_tmp[i]), "%s", val);
            hv[i] = hv_tmp[i];
            return 0;
        }
    }

    if (*hcount >= 64) return -1;
    hk[*hcount] = key;
    snprintf(hv_tmp[*hcount], sizeof(hv_tmp[*hcount]), "%s", val);
    hv[*hcount] = hv_tmp[*hcount];
    (*hcount)++;
    return 0;
}

static int append_template_body_fields(const RequestTemplate *tpl, TemplatePair *pairs, int *count, int max_count) {
    if (!tpl) return 0;

    for (int i = 0; i < tpl->body_count; i++) {
        if (strcmp(tpl->body[i].key, "__spread__") == 0) {
            ObjectVar *ov = find_object_var(tpl->body[i].val);
            if (!ov) {
                runtime_soft_errorf("template body spread object '%s' not found", tpl->body[i].val);
                continue;
            }
            for (int j = 0; j < ov->pair_count; j++) {
                if (add_body_kv(ov->pairs[j].key, ov->pairs[j].val, pairs, count, max_count) != 0) {
                    fprintf(stderr, "[bad] merged body exceeded capacity\n");
                    return -1;
                }
            }
            continue;
        }

        char tmp[512];
        const char *val = template_value(&tpl->body[i], tmp, sizeof tmp);
        if (add_body_kv(tpl->body[i].key, val ? val : "", pairs, count, max_count) != 0) {
            fprintf(stderr, "[bad] merged body exceeded capacity\n");
            return -1;
        }
    }

    return 0;
}

static int append_ast_body_fields(ASTNode *body, TemplatePair *pairs, int *count, int max_count) {
    for (ASTNode *p = body; p; p = p->right) {
        if (strcmp(p->value, "__spread__") == 0) {
            if (p->left && p->left->type == AST_IDENT) {
                ObjectVar *ov = find_object_var(p->left->value);
                if (!ov) {
                    runtime_soft_errorf("body spread object '%s' not found", p->left->value);
                    continue;
                }
                for (int j = 0; j < ov->pair_count; j++) {
                    if (add_body_kv(ov->pairs[j].key, ov->pairs[j].val, pairs, count, max_count) != 0) {
                        fprintf(stderr, "[bad] merged body exceeded capacity\n");
                        return -1;
                    }
                }
                continue;
            }

            if (p->left && p->left->type == AST_OBJECT) {
                if (append_ast_body_fields(p->left->left, pairs, count, max_count) != 0) {
                    return -1;
                }
                continue;
            }

            runtime_soft_errorf("body spread expects object variable or object literal");
            continue;
        }

        if (p->left && p->left->type == AST_OBJECT) {
            runtime_soft_errorf("nested object values are not yet supported in body fields; use spread instead");
            continue;
        }

        char tmp[512];
        const char *val = resolve(p->left, tmp, sizeof tmp);
        if (add_body_kv(p->value, val ? val : "", pairs, count, max_count) != 0) {
            fprintf(stderr, "[bad] merged body exceeded capacity\n");
            return -1;
        }
    }

    return 0;
}

static char *build_body_json_from_fields(const TemplatePair *pairs, int count) {
    char *buf = malloc(4096);
    if (!buf) return NULL;

    buf[0] = '{';
    buf[1] = '\0';

    int first = 1;
    for (int i = 0; i < count; i++) {
        append_body_json_field(buf, 4096, &first, pairs[i].key, pairs[i].val);
    }

    strncat(buf, "}", 4096 - strlen(buf) - 1);
    return buf;
}

static char *build_template_override_body_json(const RequestTemplate *tpl, ASTNode *override_body) {
    TemplatePair merged[128];
    int count = 0;
    memset(merged, 0, sizeof(merged));

    if (append_template_body_fields(tpl, merged, &count, 128) != 0) return NULL;
    if (append_ast_body_fields(override_body, merged, &count, 128) != 0) return NULL;

    return build_body_json_from_fields(merged, count);
}

static int append_body_json_field(char *buf, size_t bufsz, int *first, const char *key, const char *val) {
    if (!buf || !first || !key || !val) return -1;

    int is_raw = 0;
    if (strcmp(val, "null") == 0 || strcmp(val, "true") == 0 || strcmp(val, "false") == 0) {
        is_raw = 1;
    } else {
        const char *c = val;
        is_raw = 1;
        if (*c == '-') c++;
        if (!*c) is_raw = 0;
        for (; *c; c++) {
            if (!isdigit((unsigned char)*c) && *c != '.') {
                is_raw = 0;
                break;
            }
        }
    }

    char entry[1024];
    if (is_raw) snprintf(entry, sizeof entry, "%s\"%s\":%s", *first ? "" : ",", key, val);
    else snprintf(entry, sizeof entry, "%s\"%s\":\"%s\"", *first ? "" : ",", key, val);
    strncat(buf, entry, bufsz - strlen(buf) - 1);
    *first = 0;
    return 0;
}

static void store_object_var(const char *name, ASTNode *object_node) {
    if (!name || !name[0] || !object_node || object_node->type != AST_OBJECT) return;

    ObjectVar *ov = find_object_var(name);
    if (!ov) {
        if (g_object_var_count >= (int)(sizeof(g_object_vars) / sizeof(g_object_vars[0]))) {
            fprintf(stderr, "[bad] object variable store full\n");
            return;
        }
        ov = &g_object_vars[g_object_var_count++];
        memset(ov, 0, sizeof(*ov));
        strncpy(ov->name, name, sizeof(ov->name) - 1);
        ov->name[sizeof(ov->name) - 1] = '\0';
    }

    ov->pair_count = 0;
    for (ASTNode *p = object_node->left; p; p = p->right) {
        if (strcmp(p->value, "__spread__") == 0) {
            if (p->left && p->left->type == AST_IDENT) {
                append_object_var_pairs(p->left->value, ov->pairs, &ov->pair_count);
            } else if (p->left && p->left->type == AST_OBJECT) {
                for (ASTNode *np = p->left->left; np; np = np->right) {
                    if (strcmp(np->value, "__spread__") == 0) continue;
                    char tmp[512];
                    const char *v = resolve(np->left, tmp, sizeof tmp);
                    append_object_pair(ov->pairs, &ov->pair_count, np->value, v ? v : "", 0);
                }
            } else {
                fprintf(stderr, "[bad] spread expects object variable or object literal\n");
            }
            continue;
        }

        char tmp[512];
        const char *v = resolve(p->left, tmp, sizeof tmp);
        append_object_pair(ov->pairs, &ov->pair_count, p->value, v ? v : "", 0);
    }
}

static void run_hook_stmts(ASTNode *stmts, const char *label) {
    if (!stmts) return;

    Response hook_resp = {0, NULL, 0};
    int has_hook_resp = 0;
    StepRecord hook_steps[128];
    int hook_step_count = 0;
    int hook_step_idx = 0;

    if (label && g_verbose) {
        printf("%s[hook]%s %s\n", cc(COL_DIM_RAW), cc(COL_RESET_RAW), label);
    }

    for (ASTNode *s = stmts; s && !g_stop_requested; s = s->right) {
        exec_runtime_stmt(s, &hook_resp, &has_hook_resp, hook_steps, &hook_step_count, &hook_step_idx);
    }

    if (has_hook_resp) response_free(&hook_resp);
}

static void run_url_hooks(UrlHook *hooks, int count, const char *url, const char *path, const char *label) {
    if (!hooks || count <= 0 || g_in_url_hook) return;

    g_in_url_hook = 1;
    for (int i = 0; i < count; i++) {
        const char *pattern = hooks[i].pattern;
        if (!pattern[0]) continue;

        int matched = wildcard_match(pattern, url ? url : "") || wildcard_match(pattern, path ? path : "");
        if (matched) run_hook_stmts(hooks[i].stmts, label);
    }
    g_in_url_hook = 0;
}

static void trigger_error_hooks(const char *kind, const char *url) {
    if (g_in_error_hook) return;

    const char *path = extract_url_path(url ? url : "");

    g_in_error_hook = 1;
    run_hook_stmts(g_on_error, "on_error");

    if (kind && strcmp(kind, "assertion") == 0) {
        run_hook_stmts(g_on_assertion_error, "on_assertion_error");
    }
    if (kind && strcmp(kind, "network") == 0) {
        run_hook_stmts(g_on_network_error, "on_network_error");
    }
    if (url && url[0]) {
        run_url_hooks(g_on_url_error_hooks, g_on_url_error_hook_count, url, path, "on_url_error");
    }
    g_in_error_hook = 0;
}

static const char *import_alias_for(const char *map_csv, const char *name, char *out, size_t outsz) {
    if (!map_csv || !map_csv[0] || !name || !name[0]) return NULL;

    char buf[512];
    strncpy(buf, map_csv, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tok = strtok(buf, ",");
    while (tok) {
        while (*tok == ' ' || *tok == '\t') tok++;
        char *eq = strchr(tok, '=');
        if (!eq) {
            tok = strtok(NULL, ",");
            continue;
        }

        *eq = '\0';
        char *src = tok;
        char *dst = eq + 1;

        while (*dst == ' ' || *dst == '\t') dst++;
        char *src_end = src + strlen(src) - 1;
        while (src_end >= src && (*src_end == ' ' || *src_end == '\t')) *src_end-- = '\0';
        char *dst_end = dst + strlen(dst) - 1;
        while (dst_end >= dst && (*dst_end == ' ' || *dst_end == '\t')) *dst_end-- = '\0';

        if (strcmp(src, name) == 0) {
            strncpy(out, dst, outsz - 1);
            out[outsz - 1] = '\0';
            return out;
        }
        tok = strtok(NULL, ",");
    }
    return NULL;
}

static const RequestTemplate *find_template(const char *name) {
    for (int i = 0; i < g_template_count; i++) {
        if (strcmp(g_templates[i].name, name) == 0) return &g_templates[i];
    }
    return NULL;
}

static const char *template_value(const TemplatePair *p, char *tmp, size_t tmpsz) {
    if (!p) return "";
    if (!p->is_var) return p->val;
    const char *v = var_get(p->val);
    if (!v) {
        runtime_soft_errorf("undefined variable '%s' in request template", p->val);
        return "";
    }
    if (strcmp(v, "__object__") == 0 && find_object_var(p->val)) {
        runtime_soft_errorf("object variable '%s' cannot be used as scalar in request template; use spread", p->val);
        return "";
    }
    snprintf(tmp, tmpsz, "%s", v);
    return tmp;
}

static char *build_template_body_json(const RequestTemplate *tpl) {
    char *buf = malloc(4096);
    if (!buf) return NULL;
    buf[0] = '{';
    buf[1] = '\0';

    int first = 1;

    for (int i = 0; i < tpl->body_count; i++) {
        if (strcmp(tpl->body[i].key, "__spread__") == 0) {
            ObjectVar *ov = find_object_var(tpl->body[i].val);
            if (!ov) {
                runtime_soft_errorf("template body spread object '%s' not found", tpl->body[i].val);
                continue;
            }
            for (int j = 0; j < ov->pair_count; j++) {
                append_body_json_field(buf, 4096, &first, ov->pairs[j].key, ov->pairs[j].val);
            }
            continue;
        }

        char tmp[512];
        const char *val = template_value(&tpl->body[i], tmp, sizeof tmp);
        append_body_json_field(buf, 4096, &first, tpl->body[i].key, val ? val : "");
    }

    strncat(buf, "}", 4096 - strlen(buf) - 1);
    return buf;
}

static void register_template(ASTNode *stmt, const char *store_name) {
    if (!stmt || stmt->type != AST_REQUEST_TEMPLATE) return;
    if (!store_name || !store_name[0]) return;

    int idx = -1;
    for (int i = 0; i < g_template_count; i++) {
        if (strcmp(g_templates[i].name, store_name) == 0) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        if (g_template_count >= 256) {
            fprintf(stderr, "[bad] template store full\n");
            return;
        }
        idx = g_template_count++;
    }

    RequestTemplate *tpl = &g_templates[idx];
    ast_free(tpl->expects);
    memset(tpl, 0, sizeof(*tpl));
    strncpy(tpl->name, store_name, sizeof(tpl->name) - 1);
    strncpy(tpl->method, stmt->op, sizeof(tpl->method) - 1);
    tpl->is_export = stmt->is_export;

    if (stmt->left) {
        if (stmt->left->type == AST_IDENT) {
            tpl->path_is_var = 1;
            strncpy(tpl->path, stmt->left->value, sizeof(tpl->path) - 1);
        } else {
            char tmp[512];
            const char *v = resolve(stmt->left, tmp, sizeof tmp);
            tpl->path_is_var = 0;
            strncpy(tpl->path, v, sizeof(tpl->path) - 1);
        }
        tpl->path[sizeof(tpl->path) - 1] = '\0';
    }

    for (ASTNode *p = stmt->body; p && tpl->body_count < 64; p = p->right) {
        TemplatePair *bp = &tpl->body[tpl->body_count++];
        strncpy(bp->key, p->value, sizeof(bp->key) - 1);
        bp->key[sizeof(bp->key) - 1] = '\0';

        if (strcmp(p->value, "__spread__") == 0 && p->left && p->left->type == AST_IDENT) {
            bp->is_var = 0;
            strncpy(bp->val, p->left->value, sizeof(bp->val) - 1);
            bp->val[sizeof(bp->val) - 1] = '\0';
            continue;
        }

        if (p->left && p->left->type == AST_IDENT) {
            bp->is_var = 1;
            strncpy(bp->val, p->left->value, sizeof(bp->val) - 1);
            bp->val[sizeof(bp->val) - 1] = '\0';
        } else {
            char tmp[512];
            const char *v = resolve(p->left, tmp, sizeof tmp);
            bp->is_var = 0;
            strncpy(bp->val, v, sizeof(bp->val) - 1);
            bp->val[sizeof(bp->val) - 1] = '\0';
        }
    }

    for (ASTNode *h = stmt->headers; h && tpl->header_count < 64; h = h->right) {
        TemplatePair *hp = &tpl->headers[tpl->header_count++];
        strncpy(hp->key, h->value, sizeof(hp->key) - 1);
        hp->key[sizeof(hp->key) - 1] = '\0';

        if (strcmp(h->value, "__spread__") == 0 && h->left && h->left->type == AST_IDENT) {
            hp->is_var = 0;
            strncpy(hp->val, h->left->value, sizeof(hp->val) - 1);
            hp->val[sizeof(hp->val) - 1] = '\0';
            continue;
        }

        if (h->left && h->left->type == AST_IDENT) {
            hp->is_var = 1;
            strncpy(hp->val, h->left->value, sizeof(hp->val) - 1);
            hp->val[sizeof(hp->val) - 1] = '\0';
        } else {
            char tmp[512];
            const char *v = resolve(h->left, tmp, sizeof tmp);
            hp->is_var = 0;
            strncpy(hp->val, v, sizeof(hp->val) - 1);
            hp->val[sizeof(hp->val) - 1] = '\0';
        }
    }

    if (stmt->retry) {
        char tmp[64];
        const char *v = resolve(stmt->retry, tmp, sizeof tmp);
        tpl->has_retry_count = 1;
        tpl->retry_count = atoi(v);
        if (tpl->retry_count < 0) tpl->retry_count = 0;
    }

    if (stmt->retry_delay) {
        char tmp[64];
        const char *v = resolve(stmt->retry_delay, tmp, sizeof tmp);
        tpl->has_retry_delay_ms = 1;
        tpl->retry_delay_ms = atoi(v);
        if (tpl->retry_delay_ms < 0) tpl->retry_delay_ms = 0;
    }

    if (stmt->retry_backoff) {
        const char *mode_val = NULL;
        char tmp[64];
        if (stmt->retry_backoff->type == AST_IDENT || stmt->retry_backoff->type == AST_STRING) {
            mode_val = stmt->retry_backoff->value;
        } else {
            mode_val = resolve(stmt->retry_backoff, tmp, sizeof tmp);
        }
        tpl->has_retry_backoff = 1;
        tpl->retry_backoff_mode = parse_retry_backoff_mode(mode_val);
    }

    if (stmt->retry_jitter) {
        char tmp[64];
        const char *v = resolve(stmt->retry_jitter, tmp, sizeof tmp);
        tpl->has_retry_jitter_ms = 1;
        tpl->retry_jitter_ms = atoi(v);
        if (tpl->retry_jitter_ms < 0) tpl->retry_jitter_ms = 0;
    }

    tpl->expects = clone_ast(stmt->stmts);
}

static void exec_runtime_stmt(ASTNode *stmt, Response *resp, int *has_resp,
                              StepRecord *steps, int *step_count, int *step_idx) {
    if (!stmt || g_stop_requested) return;
    (*step_idx)++;

    if (stmt->type == AST_SEND) {
        if (*has_resp) response_free(resp);
        *resp = exec_send(stmt);
        *has_resp = 1;
        g_last_resp = *resp;
        snapshot_last_response(resp);
        g_total_req_ms += resp->time_ms;
        g_total_req_count++;

        if (g_remember_token && resp->body) {
            char *tok = json_get_path(resp->body, "token");
            if (tok && tok[0]) var_set("__auth_token__", tok);
            free(tok);
        }

        char d[512];
        snprintf(d, sizeof d, "send %s %s -> %d (%ldms)", g_last_req.method, g_last_req.url, resp->status, resp->time_ms);
        step_add(steps, step_count, *step_idx, "send", resp->status > 0 && resp->status < 600, d);

        if (g_show_time) {
            printf("    %stime_ms%s %ld\n", cc(COL_DIM_RAW), cc(COL_RESET_RAW), resp->time_ms);
        }

        if ((g_verbose || g_print_response) && resp->body) {
            printf("  %sresponse:%s\n", cc(COL_DIM_RAW), cc(COL_RESET_RAW));
            if (g_json_pretty) fmt_print_json_pretty(resp->body);
            else if (g_json_view) fmt_print_tree(resp->body, 2);
            else if (g_flat) fmt_print_flat(resp->body, "  ");
            else if (g_table) fmt_print_table(resp->body);
            else fmt_print_tree(resp->body, 2);
        }
    } else if (stmt->type == AST_EXPECT_STATUS || stmt->type == AST_EXPECT_JSON || stmt->type == AST_EXPECT_TIME) {
        char d[512] = {0};
        int ok = exec_expect(stmt, resp, d, sizeof d);
        step_add(steps, step_count, *step_idx, "expect", ok, d);
    } else if (stmt->type == AST_IF) {
        int branch = eval_condition(stmt->left);
        char d[512];
        snprintf(d, sizeof d, "if -> %s", branch ? "then" : "else");
        step_add(steps, step_count, *step_idx, "if", 1, d);

        ASTNode *branch_head = branch ? stmt->stmts : stmt->alt;
        for (ASTNode *inner = branch_head; inner && !g_stop_requested && !g_stop_current_test; inner = inner->right) {
            exec_runtime_stmt(inner, resp, has_resp, steps, step_count, step_idx);
        }
    } else if (stmt->type == AST_SKIP_IF) {
        int should_skip = eval_condition(stmt->left);
        if (should_skip) {
            g_stop_current_test = 1;
            if (stmt->skip_reason[0]) {
                printf("    %sskip_if%s because \"%s\"\n", cc(COL_YELLOW_RAW), cc(COL_RESET_RAW), stmt->skip_reason);
            } else {
                printf("    %sskip_if%s triggered\n", cc(COL_YELLOW_RAW), cc(COL_RESET_RAW));
            }
        }

        char d[512];
        snprintf(d, sizeof d, "skip_if -> %s", should_skip ? "skipped" : "continued");
        step_add(steps, step_count, *step_idx, "skip_if", 1, d);
    } else if (stmt->type == AST_FAIL_IF) {
        int should_fail = eval_condition(stmt->left);
        if (should_fail) {
            g_fail++;
            g_assertion_failures++;
            trigger_error_hooks("assertion", g_last_req.url[0] ? g_last_req.url : NULL);
            g_stop_current_test = 1;
            if (g_fail_fast) g_stop_requested = 1;
            if (stmt->skip_reason[0]) {
                printf("    %sFAIL%s fail_if because \"%s\"\n", cc(COL_RED_RAW), cc(COL_RESET_RAW), stmt->skip_reason);
            } else {
                printf("    %sFAIL%s fail_if condition matched\n", cc(COL_RED_RAW), cc(COL_RESET_RAW));
            }
        }

        char d[512];
        snprintf(d, sizeof d, "fail_if -> %s", should_fail ? "failed" : "continued");
        step_add(steps, step_count, *step_idx, "fail_if", !should_fail, d);
    } else if (stmt->type == AST_STOP) {
        g_stop_current_test = 1;
        if (stmt->skip_reason[0]) {
            printf("    %sstop%s because \"%s\"\n", cc(COL_YELLOW_RAW), cc(COL_RESET_RAW), stmt->skip_reason);
        } else {
            printf("    %sstop%s current test\n", cc(COL_YELLOW_RAW), cc(COL_RESET_RAW));
        }

        char d[512];
        snprintf(d, sizeof d, "stop current test");
        step_add(steps, step_count, *step_idx, "stop", 1, d);
    } else if (stmt->type == AST_STOP_ALL) {
        g_stop_current_test = 1;
        g_stop_requested = 1;
        g_stopped_by_user = 1;
        if (stmt->skip_reason[0]) {
            printf("    %sstop_all%s because \"%s\"\n", cc(COL_RED_RAW), cc(COL_RESET_RAW), stmt->skip_reason);
        } else {
            printf("    %sstop_all%s requested\n", cc(COL_RED_RAW), cc(COL_RESET_RAW));
        }

        char d[512];
        snprintf(d, sizeof d, "stop all execution");
        step_add(steps, step_count, *step_idx, "stop_all", 1, d);
    } else if (stmt->type == AST_TIME_START) {
        int ok = timer_start(stmt->value) == 0;
        if (ok) {
            printf("    %stime_start%s %s\n", cc(COL_DIM_RAW), cc(COL_RESET_RAW), stmt->value);
        } else {
            printf("    %sFAIL%s cannot start timer '%s'\n", cc(COL_RED_RAW), cc(COL_RESET_RAW), stmt->value);
        }

        char d[512];
        snprintf(d, sizeof d, "time_start %s", stmt->value);
        step_add(steps, step_count, *step_idx, "time_start", ok, d);
    } else if (stmt->type == AST_TIME_STOP) {
        long elapsed = 0;
        int ok = timer_stop(stmt->value, &elapsed) == 0;
        if (ok) {
            char tbuf[64];
            snprintf(tbuf, sizeof(tbuf), "%ld", elapsed);
            var_set("last_timer_ms", tbuf);
            printf("    %stime_stop%s %s -> %ldms\n", cc(COL_DIM_RAW), cc(COL_RESET_RAW), stmt->value, elapsed);
        } else {
            printf("    %sFAIL%s timer '%s' is not running\n", cc(COL_RED_RAW), cc(COL_RESET_RAW), stmt->value);
        }

        char d[512];
        snprintf(d, sizeof d, "time_stop %s", stmt->value);
        step_add(steps, step_count, *step_idx, "time_stop", ok, d);
    } else if (stmt->type == AST_SLEEP) {
        char tbuf[64];
        const char *sv = resolve(stmt->left, tbuf, sizeof tbuf);
        int ms = atoi(sv ? sv : "0");
        if (ms < 0) ms = 0;
        bad_sleep_ms(ms);
        printf("    %ssleep%s %dms\n", cc(COL_DIM_RAW), cc(COL_RESET_RAW), ms);

        char d[512];
        snprintf(d, sizeof d, "sleep %dms", ms);
        step_add(steps, step_count, *step_idx, "sleep", 1, d);
    } else if (stmt->type == AST_PRINT) {
        char tmp[512];
        const char *val = resolve(stmt->left, tmp, sizeof tmp);
        printf("    %sprint%s %s\n", cc(COL_DIM_RAW), cc(COL_RESET_RAW), val ? val : "");

        char d[512];
        snprintf(d, sizeof d, "print %s", val ? val : "");
        step_add(steps, step_count, *step_idx, "print", 1, d);
    } else if (stmt->type == AST_LET) {
        if (stmt->left && stmt->left->type == AST_SEND) {
            Response r = exec_send(stmt->left);
            var_set(stmt->value, r.body ? r.body : "");
            snapshot_last_response(&r);
            g_total_req_ms += r.time_ms;
            g_total_req_count++;

            if (g_remember_token && r.body) {
                char *tok = json_get_path(r.body, "token");
                if (tok && tok[0]) var_set("__auth_token__", tok);
                free(tok);
            }

            char d[512];
            snprintf(d, sizeof d, "let %s = send %s %s -> %d (%ldms)", stmt->value, g_last_req.method, g_last_req.url, r.status, r.time_ms);
            step_add(steps, step_count, *step_idx, "let", r.status > 0 && r.status < 600, d);

            if (g_show_time) {
                printf("    %stime_ms%s %ld\n", cc(COL_DIM_RAW), cc(COL_RESET_RAW), r.time_ms);
            }

            if (!*has_resp) {
                *resp = r;
                *has_resp = 1;
                g_last_resp = *resp;
                snapshot_last_response(resp);
            } else {
                response_free(&r);
            }
        } else {
            if (stmt->left && stmt->left->type == AST_OBJECT) {
                store_object_var(stmt->value, stmt->left);
                var_set(stmt->value, "__object__");

                char d[512];
                snprintf(d, sizeof d, "let %s = {object}", stmt->value);
                step_add(steps, step_count, *step_idx, "let", 1, d);
            } else {
                char tmp[512];
                const char *val = resolve(stmt->left, tmp, sizeof tmp);
                var_set(stmt->value, val);

                char d[512];
                snprintf(d, sizeof d, "let %s = %s", stmt->value, val ? val : "");
                step_add(steps, step_count, *step_idx, "let", 1, d);
            }
        }
    }
}

static int subtree_has_only_tests(ASTNode *nodes) {
    for (ASTNode *n = nodes; n; n = n->right) {
        ASTNode *s = n->left;
        if (!s) continue;
        if (s->type == AST_TEST && s->is_only) return 1;
        if (s->type == AST_GROUP) {
            for (ASTNode *gs = s->stmts; gs; gs = gs->right) {
                if (gs->type == AST_TEST && gs->is_only) return 1;
            }
        }
    }
    return 0;
}

static int subtree_has_only_groups(ASTNode *nodes) {
    for (ASTNode *n = nodes; n; n = n->right) {
        ASTNode *s = n->left;
        if (!s) continue;
        if (s->type == AST_GROUP && s->is_only) return 1;
        if (s->type == AST_GROUP) {
            ASTNode *curr = s->stmts;
            while (curr) {
                ASTNode container = {0};
                container.left = curr;
                container.right = NULL;
                if (subtree_has_only_groups(&container)) return 1;
                curr = curr->right;
            }
        }
    }
    return 0;
}

static int csv_contains_value(const char *csv, const char *value) {
    if (!csv || !csv[0] || !value || !value[0]) return 0;

    char buf[512];
    strncpy(buf, csv, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tok = strtok(buf, ",");
    while (tok) {
        while (*tok == ' ' || *tok == '\t') tok++;
        char *end = tok + strlen(tok) - 1;
        while (end >= tok && (*end == ' ' || *end == '\t')) *end-- = '\0';
        if (strcmp(tok, value) == 0) return 1;
        tok = strtok(NULL, ",");
    }
    return 0;
}

static int subtree_has_only_imports(ASTNode *nodes) {
    for (ASTNode *n = nodes; n; n = n->right) {
        ASTNode *s = n->left;
        if (!s) continue;
        if (s->type == AST_IMPORT && s->is_only) return 1;
        if (s->type == AST_GROUP) {
            ASTNode *curr = s->stmts;
            while (curr) {
                ASTNode container = {0};
                container.left = curr;
                container.right = NULL;
                if (subtree_has_only_imports(&container)) return 1;
                curr = curr->right;
            }
        }
    }
    return 0;
}

static void snapshot_last_response(const Response *r) {
    if (!r) {
        g_has_last_resp = 0;
        g_last_resp_status = 0;
        g_last_resp_time_ms = 0;
        g_last_resp_body[0] = '\0';
        return;
    }

    g_has_last_resp = 1;
    g_last_resp_status = r->status;
    g_last_resp_time_ms = r->time_ms;

    char tbuf[64];
    snprintf(tbuf, sizeof(tbuf), "%ld", g_last_resp_time_ms);
    var_set("last_time_ms", tbuf);
    var_set("__last_time_ms__", tbuf);

    if (r->body) {
        strncpy(g_last_resp_body, r->body, sizeof(g_last_resp_body) - 1);
        g_last_resp_body[sizeof(g_last_resp_body) - 1] = '\0';
    } else {
        g_last_resp_body[0] = '\0';
    }
}

static const char *resolve(ASTNode *n, char *tmp, size_t tmpsz) {
    if (!n) return "";
    switch (n->type) {
        case AST_STRING: return n->value;
        case AST_INT:    snprintf(tmp, tmpsz, "%d", n->int_val); return tmp;
        case AST_FLOAT:  snprintf(tmp, tmpsz, "%g", n->float_val); return tmp;
        case AST_BOOL:   return n->bool_val ? "true" : "false";
        case AST_NULL:   return "null";
        case AST_STATUS_REF: {
            if (!g_has_last_resp) {
                runtime_soft_errorf("cannot resolve status before any response");
                return "";
            }
            snprintf(tmp, tmpsz, "%d", g_last_resp_status);
            return tmp;
        }
        case AST_JSON_PATH: {
            if (!g_has_last_resp || !g_last_resp_body[0]) {
                runtime_soft_errorf("cannot resolve json.%s before any response body",
                                   n->value[0] ? n->value : "");
                return "";
            }

            if (!n->value[0]) {
                snprintf(tmp, tmpsz, "%s", g_last_resp_body);
                return tmp;
            }

            char *extracted = json_get_path(g_last_resp_body, n->value);
            if (!extracted) {
                runtime_soft_errorf("json path not found for let: json.%s", n->value);
                return "";
            }

            snprintf(tmp, tmpsz, "%s", extracted);
            free(extracted);
            return tmp;
        }
        case AST_BEARER: {
            char inner[512];
            const char *v = resolve(n->left, inner, sizeof inner);
            if (!v || !v[0]) return "";
            if (strncmp(v, "Bearer ", 7) == 0) {
                snprintf(tmp, tmpsz, "%s", v);
            } else {
                snprintf(tmp, tmpsz, "Bearer %s", v);
            }
            return tmp;
        }
        case AST_ENV_REF: {
            const char *v = getenv(n->value);
            return v ? v : "";
        }
        case AST_ARG_REF: {
            int idx = n->int_val;
            if (idx < 0 || idx >= g_arg_count) return "";
            return g_args[idx];
        }
        case AST_TIME_MS_REF: {
            if (!g_has_last_resp) {
                runtime_soft_errorf("cannot resolve time_ms before any response");
                return "";
            }
            snprintf(tmp, tmpsz, "%ld", g_last_resp_time_ms);
            return tmp;
        }
        case AST_TIMER_REF: {
            long elapsed = 0;
            if (timer_elapsed(n->value, &elapsed) != 0) {
                runtime_soft_errorf("timer '%s' not found (did you run time_start?)", n->value);
                return "";
            }
            snprintf(tmp, tmpsz, "%ld", elapsed);
            return tmp;
        }
        case AST_NOW_MS_REF:
            snprintf(tmp, tmpsz, "%ld", epoch_ms());
            return tmp;
        case AST_STATS_REF:
            return resolve_stats_ref(n->value, tmp, tmpsz);
        case AST_OBJECT:
            runtime_soft_errorf("object literal cannot be resolved as a scalar value");
            return "";
        case AST_IDENT: {
            const char *v = var_get(n->value);
            if (v) {
                if (strcmp(v, "__object__") == 0 && find_object_var(n->value)) {
                    runtime_soft_errorf("object variable '%s' cannot be used as scalar; spread it in body/header", n->value);
                    return "";
                }
                return v;
            }
            if (strcmp(n->value, "stats") == 0) {
                return resolve_stats_ref("", tmp, tmpsz);
            }
            if (find_object_var(n->value)) {
                runtime_soft_errorf("object variable '%s' cannot be used as scalar; spread it in body/header", n->value);
                return "";
            }
            runtime_soft_errorf("undefined variable '%s'", n->value);
            return "";
        }
        default: return n->value;
    }
}

static char *build_body_json(ASTNode *pairs) {
    char *buf = malloc(4096);
    if (!buf) return NULL;

    buf[0] = '{';
    buf[1] = '\0';

    int first = 1;
    for (ASTNode *p = pairs; p; p = p->right) {
        if (strcmp(p->value, "__spread__") == 0) {
            if (p->left && p->left->type == AST_IDENT) {
                ObjectVar *ov = find_object_var(p->left->value);
                if (!ov) {
                    runtime_soft_errorf("body spread object '%s' not found", p->left->value);
                    continue;
                }
                for (int i = 0; i < ov->pair_count; i++) {
                    append_body_json_field(buf, 4096, &first, ov->pairs[i].key, ov->pairs[i].val);
                }
                continue;
            }

            if (p->left && p->left->type == AST_OBJECT) {
                for (ASTNode *ip = p->left->left; ip; ip = ip->right) {
                    if (strcmp(ip->value, "__spread__") == 0) continue;
                    char tmp[512];
                    const char *val = resolve(ip->left, tmp, sizeof tmp);
                    append_body_json_field(buf, 4096, &first, ip->value, val ? val : "");
                }
                continue;
            }

            runtime_soft_errorf("body spread expects object variable or object literal");
            continue;
        }

        if (p->left && p->left->type == AST_OBJECT) {
            runtime_soft_errorf("nested object values are not yet supported in body fields; use spread instead");
            continue;
        }

        char tmp[128];
        const char *val = resolve(p->left, tmp, sizeof tmp);
        append_body_json_field(buf, 4096, &first, p->value, val ? val : "");
    }

    strncat(buf, "}", 4096 - strlen(buf) - 1);
    return buf;
}

static void write_record(FILE *f,
                         const char *record_id,
                         const char *timestamp,
                         const char *test_name,
                         const Response *r,
                         const StepRecord *steps,
                         int step_count,
                         int test_failed) {
    int req_body_truncated = 0;
    int resp_body_truncated = 0;
    char *req_body_limited = NULL;
    char *resp_body_limited = NULL;
    char *eid = json_escape(record_id);
    char *ets = json_escape(timestamp);
    char *esrc = json_escape(g_source_file);
    char *et = json_escape(test_name);
    char *em = json_escape(g_last_req.method);
    char *eu = json_escape(g_last_req.url);
    char *eb = NULL;

    if (g_history_include_request_body && g_last_req.has_body) {
        req_body_limited = dup_with_body_limit(g_last_req.body, g_history_max_body_bytes, &req_body_truncated);
        if (!req_body_limited) req_body_limited = BAD_STRDUP("");
    }

    if (g_history_include_response_body && r && r->body) {
        resp_body_limited = dup_with_body_limit(r->body, g_history_max_body_bytes, &resp_body_truncated);
        if (!resp_body_limited) resp_body_limited = BAD_STRDUP("");
    }

    eb = json_escape(req_body_limited ? req_body_limited : "");

    fprintf(f, "{\n");
    fprintf(f, "  \"schema\": \"bad-history-v2\",\n");
    fprintf(f, "  \"id\": \"%s\",\n", eid ? eid : "");
    fprintf(f, "  \"timestamp\": \"%s\",\n", ets ? ets : "");
    fprintf(f, "  \"source_file\": \"%s\",\n", esrc ? esrc : "");
    fprintf(f, "  \"test\": \"%s\",\n", et ? et : "");
    fprintf(f, "  \"test_failed\": %s,\n", test_failed ? "true" : "false");
    fprintf(f, "  \"request\": {\n");
    fprintf(f, "    \"method\": \"%s\",\n", em ? em : "");
    fprintf(f, "    \"url\": \"%s\",\n", eu ? eu : "");
    fprintf(f, "    \"timeout_ms\": %d,\n", g_timeout_ms);
    fprintf(f, "    \"headers\": ");
    if (g_history_include_headers) {
        fprintf(f, "{\n");
        for (int i = 0; i < g_last_req.hdr_count; i++) {
            char *hk = json_escape(g_last_req.hdr_keys[i]);
            char *hv = json_escape(g_last_req.hdr_vals[i]);
            fprintf(f, "      \"%s\": \"%s\"%s\n",
                    hk ? hk : "", hv ? hv : "", (i == g_last_req.hdr_count - 1) ? "" : ",");
            free(hk);
            free(hv);
        }
        fprintf(f, "    },\n");
    } else {
        fprintf(f, "{},\n");
    }

    if (g_last_req.has_body && g_history_include_request_body) {
        fprintf(f, "    \"body_raw\": \"%s\",\n", eb ? eb : "");
        fprintf(f, "    \"body_truncated\": %s\n", req_body_truncated ? "true" : "false");
    } else {
        fprintf(f, "    \"body_raw\": null,\n");
        fprintf(f, "    \"body_truncated\": false\n");
    }
    fprintf(f, "  },\n");

    fprintf(f, "  \"response\": {\n");
    fprintf(f, "    \"status\": %d,\n", r ? r->status : -1);
    fprintf(f, "    \"time_ms\": %ld,\n", r ? r->time_ms : 0L);
    fprintf(f, "    \"body\": ");
    if (g_history_include_response_body && resp_body_limited && !resp_body_truncated && has_json_shape(resp_body_limited)) {
        fprintf(f, "%s,\n", resp_body_limited);
    } else if (g_history_include_response_body && resp_body_limited) {
        char *erb = json_escape(resp_body_limited);
        fprintf(f, "\"%s\",\n", erb ? erb : "");
        free(erb);
    } else if (g_history_include_response_body && r && !r->body) {
        fprintf(f, "null,\n");
    } else {
        fprintf(f, "null,\n");
    }

    fprintf(f, "    \"body_truncated\": %s\n", resp_body_truncated ? "true" : "false");
    fprintf(f, "  }");

    if (g_save_steps) {
        fprintf(f, ",\n  \"steps\": [\n");
        for (int i = 0; i < step_count; i++) {
            char *ek = json_escape(steps[i].kind);
            char *ed = json_escape(steps[i].detail);
            fprintf(f, "    {\"index\": %d, \"kind\": \"%s\", \"ok\": %s, \"detail\": \"%s\"}%s\n",
                    steps[i].index,
                    ek ? ek : "",
                    steps[i].ok ? "true" : "false",
                    ed ? ed : "",
                    (i == step_count - 1) ? "" : ",");
            free(ek);
            free(ed);
        }
        fprintf(f, "  ]");
    }

    fprintf(f, "\n}\n");

    free(eid);
    free(ets);
    free(esrc);
    free(et);
    free(em);
    free(eu);
    free(eb);
    free(req_body_limited);
    free(resp_body_limited);
}

static void save_history(const char *test_name, const Response *r,
                         const StepRecord *steps, int step_count,
                         int test_failed) {
    FILE *f;
    time_t t;
    struct tm *tm;
    char fname[1024];
    char record_id[128];
    char timestamp[64];
    char tname[80] = {0};
    char srcname[256] = {0};
    int mode_per_file;
    int mode_per_test;

    if (!should_save_history_record(test_failed)) {
        return;
    }

    mode_per_file = BAD_STRCASECMP(g_history_mode, "per-file") == 0 ||
                    BAD_STRCASECMP(g_history_mode, "per_file") == 0 ||
                    BAD_STRCASECMP(g_history_mode, "file") == 0;
    mode_per_test = BAD_STRCASECMP(g_history_mode, "per-test") == 0 ||
                    BAD_STRCASECMP(g_history_mode, "per_test") == 0 ||
                    BAD_STRCASECMP(g_history_mode, "test") == 0;

    now_iso8601(timestamp, sizeof timestamp);
    snprintf(record_id, sizeof(record_id), "%ld-%d", (long)time(NULL), ++g_record_seq);

    if (mode_per_file || mode_per_test) {
        if (ensure_dir(g_history_dir) != 0) {
            fprintf(stderr, "[bad] cannot create history dir: %s\n", g_history_dir);
            return;
        }

        sanitize_name(srcname, sizeof(srcname), source_file_basename());
        if (!srcname[0]) strncpy(srcname, "suite", sizeof(srcname) - 1);

        if (mode_per_file) {
            snprintf(fname, sizeof fname, "%s/%s.jsonl", g_history_dir, srcname);
        } else {
            sanitize_name(tname, sizeof tname, test_name);
            snprintf(fname, sizeof fname, "%s/%s/%s.jsonl", g_history_dir, srcname, tname);
        }

        if (ensure_parent_dir(fname) != 0) {
            fprintf(stderr, "[bad] cannot create parent dir for history file: %s\n", fname);
            return;
        }

        if (strcmp(g_history_format, "jsonl") != 0) {
            fprintf(stderr, "[bad] unsupported history format '%s' for mode '%s', using jsonl\n", g_history_format, g_history_mode);
        }

        f = fopen(fname, "a");
        if (!f) return;
        write_record(f, record_id, timestamp, test_name, r, steps, step_count, test_failed);
        fclose(f);
        printf("%s  saved -> %s%s\n", cc(COL_DIM_RAW), fname, cc(COL_RESET_RAW));
        return;
    }

    if (g_history_file[0]) {
        if (ensure_parent_dir(g_history_file) != 0) {
            fprintf(stderr, "[bad] cannot create parent dir for history file: %s\n", g_history_file);
            return;
        }
        if (strcmp(g_history_format, "jsonl") != 0) {
            fprintf(stderr, "[bad] unsupported history format '%s', using jsonl\n", g_history_format);
        }
        f = fopen(g_history_file, "a");
        if (!f) return;
        write_record(f, record_id, timestamp, test_name, r, steps, step_count, test_failed);
        fclose(f);
        printf("%s  saved -> %s%s\n", cc(COL_DIM_RAW), g_history_file, cc(COL_RESET_RAW));
        return;
    }

    if (ensure_dir(g_history_dir) != 0) {
        fprintf(stderr, "[bad] cannot create history dir: %s\n", g_history_dir);
        return;
    }

    t = time(NULL);
    tm = localtime(&t);
    sanitize_name(tname, sizeof tname, test_name);
    snprintf(fname, sizeof fname, "%s/%s_%04d%02d%02d_%02d%02d%02d.json",
             g_history_dir,
             tname,
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);

    f = fopen(fname, "w");
    if (!f) return;

    write_record(f, record_id, timestamp, test_name, r, steps, step_count, test_failed);

    fclose(f);

    printf("%s  saved -> %s%s\n", cc(COL_DIM_RAW), fname, cc(COL_RESET_RAW));
}

static Response exec_send(ASTNode *node) {
    char tmp[512];
    char url[1024];
    const char *hk[64], *hv[64];
    char hv_tmp[64][512];
    int hcount = 0;
    char *body_json = NULL;
    int retries = g_retry_count;
    int retry_delay = g_retry_delay_ms;
    int retry_backoff = g_retry_backoff_mode;
    int retry_jitter = g_retry_jitter_ms;

    const char *method = node->value;
    const char *path = NULL;

    RequestTemplate local_tpl;
    memset(&local_tpl, 0, sizeof local_tpl);

    if (strcmp(node->value, "__request_template__") == 0) {
        const char *tpl_name = (node->left && node->left->type == AST_IDENT) ? node->left->value : "";
        const RequestTemplate *tpl = find_template(tpl_name);
        if (!tpl) {
            Response err = {0};
            char err_msg[256];
            runtime_soft_errorf("request template '%s' not found", tpl_name[0] ? tpl_name : "");
            err.status = -1;
            snprintf(err_msg, sizeof(err_msg),
                     "{\"error\":\"request template '%s' not found\"}",
                     tpl_name[0] ? tpl_name : "");
            err.body = BAD_STRDUP(err_msg);
            g_network_failures++;
            trigger_error_hooks("network", NULL);
            return err;
        }
        local_tpl = *tpl;
        method = local_tpl.method;

        if (local_tpl.has_retry_count) retries = local_tpl.retry_count;
        if (local_tpl.has_retry_delay_ms) retry_delay = local_tpl.retry_delay_ms;
        if (local_tpl.has_retry_backoff) retry_backoff = local_tpl.retry_backoff_mode;
        if (local_tpl.has_retry_jitter_ms) retry_jitter = local_tpl.retry_jitter_ms;

        if (local_tpl.path_is_var) {
            const char *pv = var_get(local_tpl.path);
            path = pv ? pv : "";
        } else {
            path = local_tpl.path;
        }

        if (node->extra) {
            char ovtmp[512];
            path = resolve(node->extra, ovtmp, sizeof ovtmp);
            strncpy(local_tpl.path, path, sizeof(local_tpl.path) - 1);
            local_tpl.path[sizeof(local_tpl.path) - 1] = '\0';
            path = local_tpl.path;
        }

        int merge_body = 0;
        if (node->alt) {
            char mtmp[64];
            const char *mv = resolve(node->alt, mtmp, sizeof mtmp);
            merge_body = parse_bool_like(mv);
        }

        if (node->body) {
            if (merge_body && local_tpl.body_count > 0) {
                body_json = build_template_override_body_json(&local_tpl, node->body);
                if (!body_json) {
                    body_json = build_body_json(node->body);
                }
            } else {
                body_json = build_body_json(node->body);
            }
        } else if (local_tpl.body_count > 0) {
            body_json = build_template_body_json(&local_tpl);
        }

        for (int i = 0; i < local_tpl.header_count && hcount < 64; i++) {
            if (strcmp(local_tpl.headers[i].key, "__spread__") == 0) {
                ObjectVar *ov = find_object_var(local_tpl.headers[i].val);
                if (!ov) {
                    runtime_soft_errorf("template header spread object '%s' not found", local_tpl.headers[i].val);
                    continue;
                }
                for (int j = 0; j < ov->pair_count; j++) {
                    add_header_kv(ov->pairs[j].key, ov->pairs[j].val, hk, hv, hv_tmp, &hcount);
                }
                continue;
            }

            const char *vv = template_value(&local_tpl.headers[i], hv_tmp[hcount], sizeof(hv_tmp[hcount]));
            add_header_kv(local_tpl.headers[i].key, vv ? vv : "", hk, hv, hv_tmp, &hcount);
        }

        for (ASTNode *h = node->headers; h && hcount < 64; h = h->right) {
            if (strcmp(h->value, "__spread__") == 0) {
                if (h->left && h->left->type == AST_IDENT) {
                    ObjectVar *ov = find_object_var(h->left->value);
                    if (!ov) {
                        runtime_soft_errorf("header spread object '%s' not found", h->left->value);
                        continue;
                    }
                    for (int j = 0; j < ov->pair_count; j++) {
                        add_header_kv(ov->pairs[j].key, ov->pairs[j].val, hk, hv, hv_tmp, &hcount);
                    }
                    continue;
                }

                if (h->left && h->left->type == AST_OBJECT) {
                    for (ASTNode *ip = h->left->left; ip; ip = ip->right) {
                        if (strcmp(ip->value, "__spread__") == 0) continue;
                        char tmpv2[512];
                        const char *iv = resolve(ip->left, tmpv2, sizeof tmpv2);
                        add_header_kv(ip->value, iv ? iv : "", hk, hv, hv_tmp, &hcount);
                    }
                    continue;
                }

                runtime_soft_errorf("header spread expects object variable or object literal");
                continue;
            }

            char tmpv[512];
            const char *vv = resolve(h->left, tmpv, sizeof tmpv);
            add_header_kv(h->value, vv ? vv : "", hk, hv, hv_tmp, &hcount);
        }
    } else {
        path = resolve(node->left, tmp, sizeof tmp);

        if (node->body) body_json = build_body_json(node->body);

        for (ASTNode *h = node->headers; h && hcount < 64; h = h->right) {
            if (strcmp(h->value, "__spread__") == 0) {
                if (h->left && h->left->type == AST_IDENT) {
                    ObjectVar *ov = find_object_var(h->left->value);
                    if (!ov) {
                        runtime_soft_errorf("header spread object '%s' not found", h->left->value);
                        continue;
                    }
                    for (int j = 0; j < ov->pair_count; j++) {
                        add_header_kv(ov->pairs[j].key, ov->pairs[j].val, hk, hv, hv_tmp, &hcount);
                    }
                    continue;
                }

                if (h->left && h->left->type == AST_OBJECT) {
                    for (ASTNode *ip = h->left->left; ip; ip = ip->right) {
                        if (strcmp(ip->value, "__spread__") == 0) continue;
                        char tmpv2[512];
                        const char *iv = resolve(ip->left, tmpv2, sizeof tmpv2);
                        add_header_kv(ip->value, iv ? iv : "", hk, hv, hv_tmp, &hcount);
                    }
                    continue;
                }

                runtime_soft_errorf("header spread expects object variable or object literal");
                continue;
            }

            const char *vv = resolve(h->left, hv_tmp[hcount], sizeof(hv_tmp[hcount]));
            add_header_kv(h->value, vv ? vv : "", hk, hv, hv_tmp, &hcount);
        }
    }

    if (strncmp(path, "http://", 7) == 0 || strncmp(path, "https://", 8) == 0) {
        strncpy(url, path, sizeof url - 1);
        url[sizeof(url) - 1] = '\0';
    } else if (path[0] == '/') {
        snprintf(url, sizeof url, "%s%s", g_base_url, path);
    } else {
        snprintf(url, sizeof url, "%s/%s", g_base_url, path);
    }

    if (g_remember_token && hcount < 64) {
        int has_auth = 0;
        for (int i = 0; i < hcount; i++) {
            if (BAD_STRCASECMP(hk[i], "Authorization") == 0) {
                has_auth = 1;
                break;
            }
        }
        if (!has_auth) {
            const char *tok = var_get("__auth_token__");
            if (tok && tok[0]) {
                hk[hcount] = "Authorization";
                if (strncmp(tok, "Bearer ", 7) == 0) {
                    snprintf(hv_tmp[hcount], sizeof(hv_tmp[hcount]), "%s", tok);
                } else {
                    snprintf(hv_tmp[hcount], sizeof(hv_tmp[hcount]), "Bearer %s", tok);
                }
                hv[hcount] = hv_tmp[hcount];
                hcount++;
            }
        }
    }

    run_url_hooks(g_before_url_hooks, g_before_url_hook_count, url, extract_url_path(url), "before_url");

    memset(&g_last_req, 0, sizeof g_last_req);
    strncpy(g_last_req.method, method, sizeof(g_last_req.method) - 1);
    strncpy(g_last_req.url, url, sizeof(g_last_req.url) - 1);
    if (body_json) {
        g_last_req.has_body = 1;
        strncpy(g_last_req.body, body_json, sizeof(g_last_req.body) - 1);
    }
    g_last_req.hdr_count = hcount;
    for (int i = 0; i < hcount; i++) {
        strncpy(g_last_req.hdr_keys[i], hk[i], sizeof(g_last_req.hdr_keys[i]) - 1);
        strncpy(g_last_req.hdr_vals[i], hv[i], sizeof(g_last_req.hdr_vals[i]) - 1);
    }

    if (node->retry) {
        char rtmp[64];
        const char *rv = resolve(node->retry, rtmp, sizeof rtmp);
        retries = atoi(rv);
    }
    if (node->retry_delay) {
        char dtmp[64];
        const char *dv = resolve(node->retry_delay, dtmp, sizeof dtmp);
        retry_delay = atoi(dv);
    }
    if (node->retry_backoff) {
        char btmp[64];
        const char *bv = NULL;
        if (node->retry_backoff->type == AST_IDENT || node->retry_backoff->type == AST_STRING) {
            bv = node->retry_backoff->value;
        } else {
            bv = resolve(node->retry_backoff, btmp, sizeof btmp);
        }
        retry_backoff = parse_retry_backoff_mode(bv);
    }
    if (node->retry_jitter) {
        char jtmp[64];
        const char *jv = resolve(node->retry_jitter, jtmp, sizeof jtmp);
        retry_jitter = atoi(jv);
    }
    if (retries < 0) retries = 0;
    if (retry_delay < 0) retry_delay = 0;
    if (retry_jitter < 0) retry_jitter = 0;

    if (g_verbose || g_print_request || g_show_timestamp || g_log_level >= LOG_DEBUG) {
        if (g_show_timestamp) {
            char ts[64];
            now_iso8601(ts, sizeof ts);
            printf("%s    timestamp: %s%s\n", cc(COL_DIM_RAW), ts, cc(COL_RESET_RAW));
        }
        printf("%s  -> %s %s%s\n", cc(COL_DIM_RAW), method, url, cc(COL_RESET_RAW));
        printf("%s    timeout: %dms%s\n", cc(COL_DIM_RAW), g_timeout_ms, cc(COL_RESET_RAW));
        if (body_json) printf("%s    body: %s%s\n", cc(COL_DIM_RAW), body_json, cc(COL_RESET_RAW));
        if (node->retry || retries > 0) {
            printf("%s    retry: %d%s\n", cc(COL_DIM_RAW), retries, cc(COL_RESET_RAW));
            if (retry_delay > 0) {
                printf("%s    retry_delay_ms: %d%s\n", cc(COL_DIM_RAW), retry_delay, cc(COL_RESET_RAW));
            }
            printf("%s    retry_backoff: %s%s\n", cc(COL_DIM_RAW), retry_backoff_mode_name(retry_backoff), cc(COL_RESET_RAW));
            if (retry_jitter > 0) {
                printf("%s    retry_jitter_ms: %d%s\n", cc(COL_DIM_RAW), retry_jitter, cc(COL_RESET_RAW));
            }
        }
        for (int i = 0; i < hcount; i++) {
            printf("%s    header: %s: %s%s\n", cc(COL_DIM_RAW), hk[i], hv[i], cc(COL_RESET_RAW));
        }
    }

    int attempt = 0;
    for (;;) {
        Response resp = http_request(method, url, body_json, hk, hv, hcount, g_timeout_ms);
        int retryable = (resp.status < 0 || resp.status >= 500 || resp.status == 429);
        if (!retryable || attempt >= retries) {
            if (resp.status < 0) {
                g_network_failures++;
                trigger_error_hooks("network", url);
            }

            if (strcmp(node->value, "__request_template__") == 0 && local_tpl.expects && resp.status >= 0) {
                for (ASTNode *e = local_tpl.expects; e; e = e->right) {
                    if (e->type == AST_EXPECT_STATUS || e->type == AST_EXPECT_JSON || e->type == AST_EXPECT_TIME) {
                        char detail[512] = {0};
                        exec_expect(e, &resp, detail, sizeof detail);
                    }
                }
            }

            run_url_hooks(g_after_url_hooks, g_after_url_hook_count, url, extract_url_path(url), "after_url");
            free(body_json);
            return resp;
        }

        int failed_status = resp.status;
        response_free(&resp);
        attempt++;

        if (g_verbose || g_print_request || g_log_level >= LOG_DEBUG) {
            printf("%s    retry attempt %d/%d after status %d%s\n",
                   cc(COL_YELLOW_RAW), attempt, retries, failed_status, cc(COL_RESET_RAW));
        }

        int sleep_ms = compute_retry_delay_ms(retry_delay, attempt, retry_backoff, retry_jitter);
        if (sleep_ms > 0) {
            bad_sleep_ms(sleep_ms);
        }
    }
}

static int compare(const char *actual, const char *op, const char *expected) {
    char *ae, *ee;
    double ad = strtod(actual, &ae);
    double ed = strtod(expected, &ee);
    int both_num = (*ae == '\0' && *ee == '\0');

    if (strcmp(op, "==") == 0) return both_num ? ad == ed : strcmp(actual, expected) == 0;
    if (strcmp(op, "!=") == 0) return both_num ? ad != ed : strcmp(actual, expected) != 0;
    if (strcmp(op, "<") == 0) return both_num && ad < ed;
    if (strcmp(op, ">") == 0) return both_num && ad > ed;
    if (strcmp(op, "<=") == 0) return both_num && ad <= ed;
    if (strcmp(op, ">=") == 0) return both_num && ad >= ed;
    if (strcmp(op, "contains") == 0) return strstr(actual, expected) != NULL;
    if (strcmp(op, "starts_with") == 0) return strncmp(actual, expected, strlen(expected)) == 0;
    if (strcmp(op, "ends_with") == 0) {
        size_t al = strlen(actual);
        size_t el = strlen(expected);
        return el <= al && strcmp(actual + (al - el), expected) == 0;
    }
    if (strcmp(op, "regex") == 0) {
#if BAD_NO_POSIX_REGEX
        /* Fallback for environments without <regex.h> support. */
        return strstr(actual, expected) != NULL;
#else
        regex_t re;
        if (regcomp(&re, expected, REG_EXTENDED | REG_NOSUB) != 0) {
            return 0;
        }
        int ok = (regexec(&re, actual, 0, NULL, 0) == 0);
        regfree(&re);
        return ok;
#endif
    }
    return 0;
}

static int list_contains_value(ASTNode *list, const char *actual) {
    if (!list || list->type != AST_LIST) return 0;
    for (ASTNode *item = list->left; item; item = item->right) {
        char tmp[512];
        const char *candidate = resolve(item, tmp, sizeof tmp);
        if (compare(actual ? actual : "", "==", candidate ? candidate : "")) {
            return 1;
        }
    }
    return 0;
}

static int eval_condition(ASTNode *cond) {
    if (!cond) return 0;

    if (cond->type == AST_COMPARE) {
        const char *op = cond->op[0] ? cond->op : "truthy";

        if (strcmp(op, "not") == 0) {
            return !eval_condition(cond->left);
        }

        if (strcmp(op, "and") == 0) {
            return eval_condition(cond->left) && eval_condition(cond->extra);
        }

        if (strcmp(op, "or") == 0) {
            return eval_condition(cond->left) || eval_condition(cond->extra);
        }

        if (strcmp(op, "in") == 0) {
            char ltmp[512];
            const char *lv = resolve(cond->left, ltmp, sizeof ltmp);
            if (cond->extra && cond->extra->type == AST_LIST) {
                return list_contains_value(cond->extra, lv ? lv : "");
            }
            return 0;
        }

        if (strcmp(op, "exists") == 0) {
            if (cond->left && cond->left->type == AST_JSON_PATH) {
                if (!g_has_last_resp || !g_last_resp_body[0]) return 0;
                if (!cond->left->value[0]) return 1;
                return json_path_exists(g_last_resp_body, cond->left->value);
            }

            char ltmp[512];
            const char *lv = resolve(cond->left, ltmp, sizeof ltmp);
            return lv && lv[0] && strcmp(lv, "false") != 0 && strcmp(lv, "0") != 0 && strcmp(lv, "null") != 0;
        }

        if (strcmp(op, "truthy") == 0) {
            char ltmp[512];
            const char *lv = resolve(cond->left, ltmp, sizeof ltmp);
            return lv && lv[0] && strcmp(lv, "false") != 0 && strcmp(lv, "0") != 0 && strcmp(lv, "null") != 0;
        }

        char ltmp[512], rtmp[512];
        const char *lv = resolve(cond->left, ltmp, sizeof ltmp);
        const char *rv = resolve(cond->extra, rtmp, sizeof rtmp);
        return compare(lv ? lv : "", op, rv ? rv : "");
    }

    char tmp[512];
    const char *v = resolve(cond, tmp, sizeof tmp);
    return v && v[0] && strcmp(v, "false") != 0 && strcmp(v, "0") != 0 && strcmp(v, "null") != 0;
}

static int exec_expect(ASTNode *node, const Response *resp, char *detail, size_t detail_sz) {
    if (node->type == AST_EXPECT_STATUS) {
        if (!node->op[0]) {
            int ok = (resp->status == node->int_val);
            if (ok) {
                printf("    %sOK%s status %d\n", cc(COL_GREEN_RAW), cc(COL_RESET_RAW), node->int_val);
                g_pass++;
                g_assertion_passes++;
            } else {
                printf("    %sFAIL%s status - %sexpected %d, got %d%s\n",
                       cc(COL_RED_RAW), cc(COL_RESET_RAW),
                       cc(COL_RED_RAW), node->int_val, resp->status, cc(COL_RESET_RAW));
                g_fail++;
                g_assertion_failures++;
                trigger_error_hooks("assertion", g_last_req.url[0] ? g_last_req.url : NULL);
                if (g_fail_fast) g_stop_requested = 1;
            }
            snprintf(detail, detail_sz, "expect status %d", node->int_val);
            return ok;
        }

        char atmp[64];
        snprintf(atmp, sizeof atmp, "%d", resp->status);
        int ok;

        if (strcmp(node->op, "in") == 0 && node->extra && node->extra->type == AST_LIST) {
            ok = list_contains_value(node->extra, atmp);
        } else {
            char etmp[512];
            const char *expected = resolve(node->extra, etmp, sizeof etmp);
            ok = compare(atmp, node->op, expected ? expected : "");
        }

        if (ok) {
            printf("    %sOK%s status %s\n", cc(COL_GREEN_RAW), cc(COL_RESET_RAW), node->op);
            g_pass++;
            g_assertion_passes++;
        } else {
            printf("    %sFAIL%s status - %scondition '%s' failed (got %d)%s\n",
                   cc(COL_RED_RAW), cc(COL_RESET_RAW), cc(COL_RED_RAW), node->op, resp->status, cc(COL_RESET_RAW));
            g_fail++;
            g_assertion_failures++;
            trigger_error_hooks("assertion", g_last_req.url[0] ? g_last_req.url : NULL);
            if (g_fail_fast) g_stop_requested = 1;
        }
        snprintf(detail, detail_sz, "expect status %s", node->op);
        return ok;
    }

    if (node->type == AST_EXPECT_TIME) {
        char atmp[512];
        const char *actual = resolve(node->left, atmp, sizeof atmp);
        int ok;

        if (strcmp(node->op, "in") == 0 && node->extra && node->extra->type == AST_LIST) {
            ok = list_contains_value(node->extra, actual ? actual : "");
        } else {
            char etmp[512];
            const char *expected = resolve(node->extra, etmp, sizeof etmp);
            ok = compare(actual ? actual : "", node->op, expected ? expected : "");
        }

        const char *label = "time";
        if (node->left && node->left->type == AST_TIME_MS_REF) label = "time_ms";
        else if (node->left && node->left->type == AST_NOW_MS_REF) label = "now_ms";

        if (ok) {
            printf("    %sOK%s %s %s\n", cc(COL_GREEN_RAW), cc(COL_RESET_RAW), label, node->op);
            g_pass++;
            g_assertion_passes++;
        } else {
            printf("    %sFAIL%s %s - %scondition '%s' failed, got %s%s\n",
                   cc(COL_RED_RAW), cc(COL_RESET_RAW),
                   label, cc(COL_RED_RAW), node->op, actual ? actual : "(null)", cc(COL_RESET_RAW));
            g_fail++;
            g_assertion_failures++;
            trigger_error_hooks("assertion", g_last_req.url[0] ? g_last_req.url : NULL);
            if (g_fail_fast) g_stop_requested = 1;
        }

        snprintf(detail, detail_sz, "expect %s %s", label, node->op);
        return ok;
    }

    const char *path = node->value;
    const char *op = node->op;

    if (strcmp(op, "exists") == 0) {
        int ok = resp->body && json_path_exists(resp->body, path);
        if (ok) {
            printf("    %sOK%s json.%s exists\n", cc(COL_GREEN_RAW), cc(COL_RESET_RAW), path);
            g_pass++;
            g_assertion_passes++;
        } else {
            printf("    %sFAIL%s json.%s - not found\n", cc(COL_RED_RAW), cc(COL_RESET_RAW), path);
            g_fail++;
            g_assertion_failures++;
            trigger_error_hooks("assertion", g_last_req.url[0] ? g_last_req.url : NULL);
            if (g_fail_fast) g_stop_requested = 1;
        }
        snprintf(detail, detail_sz, "expect json.%s exists", path);
        return ok;
    }

    char *actual = resp->body ? json_get_path(resp->body, path) : NULL;
    int ok = 0;

    if (actual) {
        if (strcmp(op, "in") == 0 && node->extra && node->extra->type == AST_LIST) {
            ok = list_contains_value(node->extra, actual);
        } else {
            char tmp[512];
            const char *expected = resolve(node->extra, tmp, sizeof tmp);
            ok = compare(actual, op, expected ? expected : "");
        }
    }

    if (ok) {
        printf("    %sOK%s json.%s %s\n", cc(COL_GREEN_RAW), cc(COL_RESET_RAW), path, op);
        g_pass++;
        g_assertion_passes++;
    } else {
        printf("    %sFAIL%s json.%s - %scondition '%s' failed, got %s%s\n",
               cc(COL_RED_RAW), cc(COL_RESET_RAW),
               path, cc(COL_RED_RAW), op, actual ? actual : "(null)", cc(COL_RESET_RAW));
        g_fail++;
        g_assertion_failures++;
        trigger_error_hooks("assertion", g_last_req.url[0] ? g_last_req.url : NULL);
        if (g_fail_fast) g_stop_requested = 1;
    }
    free(actual);
    snprintf(detail, detail_sz, "expect json.%s %s", path, op);
    return ok;
}

static void exec_test(ASTNode *test) {
    const char *name = test->value;
    Response resp = {0, NULL, 0};
    int has_resp = 0;
    StepRecord steps[512];
    int step_count = 0;
    int step_idx = 0;

    g_pass = 0;
    g_fail = 0;
    g_stop_current_test = 0;

    if (g_only_req[0]) {
        int has_req_send = 0;
        int matched_req = 0;
        for (ASTNode *s = test->stmts; s; s = s->right) {
            if (s->type == AST_SEND && strcmp(s->value, "__request_template__") == 0 && s->left && s->left->type == AST_IDENT) {
                has_req_send = 1;
                if (csv_contains_value(g_only_req, s->left->value)) {
                    matched_req = 1;
                    break;
                }
            }
        }
        if (!has_req_send || !matched_req) {
            g_filtered_tests++;
            printf("%s  - filtered test \"%s\" (only req: %s)%s\n",
                   cc(COL_DIM_RAW), name, g_only_req, cc(COL_RESET_RAW));
            return;
        }
    }

    printf("\n%s%s◆ test \"%s\"%s\n", cc(COL_BOLD_RAW), cc(COL_CYAN_RAW), name, cc(COL_RESET_RAW));

    for (ASTNode *b = g_before_each; b && !g_stop_requested && !g_stop_current_test; b = b->right)
        exec_runtime_stmt(b, &resp, &has_resp, steps, &step_count, &step_idx);

    for (ASTNode *stmt = test->stmts; stmt && !g_stop_requested && !g_stop_current_test; stmt = stmt->right)
        exec_runtime_stmt(stmt, &resp, &has_resp, steps, &step_count, &step_idx);

    /* Always run after_each cleanup unless full execution is already stopped. */
    for (ASTNode *a = g_after_each; a && !g_stop_requested; a = a->right)
        exec_runtime_stmt(a, &resp, &has_resp, steps, &step_count, &step_idx);

    printf("  %s%s%s (%d/%d passed)%s [%ldms]\n",
           g_fail == 0 ? cc(COL_GREEN_RAW) : cc(COL_RED_RAW),
           g_fail == 0 ? "OK" : "FAIL",
           cc(COL_RESET_RAW),
           g_pass,
           g_pass + g_fail,
           cc(COL_RESET_RAW),
           resp.time_ms);

    if ((g_pass + g_fail) == 0) {
        g_zero_assert_tests++;
        printf("  %swarning%s no assertions in test \"%s\"\n",
               cc(COL_YELLOW_RAW), cc(COL_RESET_RAW), name);
    }

    if (!(g_verbose || g_print_response) && has_resp && resp.body) {
        if (g_json_pretty) fmt_print_json_pretty(resp.body);
        else if (g_json_view) fmt_print_tree(resp.body, 2);
        else if (g_flat) fmt_print_flat(resp.body, "  ");
        else if (g_table) fmt_print_table(resp.body);
    }

    if (has_resp) save_history(name, &resp, steps, step_count, g_fail > 0);
    if (has_resp) response_free(&resp);
}

int runtime_exec(ASTNode *root, const RuntimeOptions *opts) {
    RuntimeOptions defaults = runtime_options_default();
    const RuntimeOptions *o = opts ? opts : &defaults;

    g_verbose = o->verbose;
    g_save = o->save_history;
    g_flat = o->flat_mode;
    g_table = o->table_mode;
    g_print_request = o->print_request;
    g_print_response = o->print_response;
    g_use_color = o->use_color;
    g_fail_fast = o->fail_fast;
    g_strict_runtime_errors = o->strict_runtime_errors;
    g_save_steps = o->save_steps;
    g_json_view = o->json_view;
    g_json_pretty = o->json_pretty;
    g_remember_token = o->remember_token;
    g_show_time = o->show_time;
    g_show_timestamp = o->show_timestamp;
    g_log_level = parse_log_level(o->log_level);
    if (g_runtime_depth == 0) {
        g_retry_count = 0;
        g_retry_delay_ms = 0;
        g_retry_backoff_mode = 0;
        g_retry_jitter_ms = 0;
        g_assertion_passes = 0;
        g_assertion_failures = 0;
        g_network_failures = 0;
        g_soft_runtime_errors = 0;
        g_zero_assert_tests = 0;
        srand((unsigned int)time(NULL));
    }
    g_timeout_ms = o->timeout_ms > 0 ? o->timeout_ms : 10000;
    g_base_url_locked = o->base_url_overridden;
    g_timeout_locked = o->timeout_overridden;
    g_stop_requested = 0;
    g_stop_current_test = 0;
    g_stopped_by_user = 0;

    if (g_runtime_depth == 0) {
        g_skipped_tests = 0;
        g_skipped_groups = 0;
        g_filtered_tests = 0;
        g_filtered_groups = 0;
        g_only_req[0] = '\0';
        g_only_import[0] = '\0';
        g_has_last_resp = 0;
        g_last_resp_status = 0;
        g_last_resp_time_ms = 0;
        g_last_resp_body[0] = '\0';
        g_stop_current_test = 0;
        g_total_req_ms = 0;
        g_total_req_count = 0;
        g_timer_count = 0;
        clear_templates();
        clear_object_vars();
        g_on_error = NULL;
        g_on_assertion_error = NULL;
        g_on_network_error = NULL;
        g_before_url_hook_count = 0;
        g_after_url_hook_count = 0;
        g_on_url_error_hook_count = 0;
        g_in_url_hook = 0;
        g_in_error_hook = 0;
    }

    strncpy(g_base_url, o->base_url, sizeof(g_base_url) - 1);
    g_base_url[sizeof(g_base_url) - 1] = '\0';

    strncpy(g_history_dir, o->history_dir, sizeof(g_history_dir) - 1);
    g_history_dir[sizeof(g_history_dir) - 1] = '\0';
    if (!g_history_dir[0]) strncpy(g_history_dir, ".bad-history", sizeof(g_history_dir) - 1);

    strncpy(g_history_file, o->history_file, sizeof(g_history_file) - 1);
    g_history_file[sizeof(g_history_file) - 1] = '\0';

    strncpy(g_history_format, o->history_format, sizeof(g_history_format) - 1);
    g_history_format[sizeof(g_history_format) - 1] = '\0';
    if (!g_history_format[0]) strncpy(g_history_format, "jsonl", sizeof(g_history_format) - 1);

    strncpy(g_history_mode, o->history_mode, sizeof(g_history_mode) - 1);
    g_history_mode[sizeof(g_history_mode) - 1] = '\0';
    if (!g_history_mode[0]) strncpy(g_history_mode, "all", sizeof(g_history_mode) - 1);

    strncpy(g_history_methods, o->history_methods, sizeof(g_history_methods) - 1);
    g_history_methods[sizeof(g_history_methods) - 1] = '\0';

    strncpy(g_history_exclude_methods, o->history_exclude_methods, sizeof(g_history_exclude_methods) - 1);
    g_history_exclude_methods[sizeof(g_history_exclude_methods) - 1] = '\0';

    g_history_only_failed = o->history_only_failed;
    g_history_include_headers = o->history_include_headers;
    g_history_include_request_body = o->history_include_request_body;
    g_history_include_response_body = o->history_include_response_body;
    g_history_max_body_bytes = o->history_max_body_bytes;
    if (g_history_max_body_bytes < 0) g_history_max_body_bytes = 0;

    strncpy(g_source_file, o->source_file, sizeof(g_source_file) - 1);
    g_source_file[sizeof(g_source_file) - 1] = '\0';

    strncpy(g_import_only, o->import_only, sizeof(g_import_only) - 1);
    g_import_only[sizeof(g_import_only) - 1] = '\0';

    size_t bl = strlen(g_base_url);
    if (bl > 0 && g_base_url[bl - 1] == '/') g_base_url[bl - 1] = '\0';

    int total_pass = 0;
    int total_fail = 0;
    int before_all_ran = 0;

    ASTNode *saved_before_all = g_before_all;
    ASTNode *saved_before_each = g_before_each;
    ASTNode *saved_after_each = g_after_each;
    ASTNode *saved_after_all = g_after_all;
    ASTNode *saved_on_error = g_on_error;
    ASTNode *saved_on_assertion_error = g_on_assertion_error;
    ASTNode *saved_on_network_error = g_on_network_error;
    UrlHook saved_before_url_hooks[64];
    UrlHook saved_after_url_hooks[64];
    UrlHook saved_on_url_error_hooks[64];
    int saved_before_url_hook_count = g_before_url_hook_count;
    int saved_after_url_hook_count = g_after_url_hook_count;
    int saved_on_url_error_hook_count = g_on_url_error_hook_count;
    memcpy(saved_before_url_hooks, g_before_url_hooks, sizeof(saved_before_url_hooks));
    memcpy(saved_after_url_hooks, g_after_url_hooks, sizeof(saved_after_url_hooks));
    memcpy(saved_on_url_error_hooks, g_on_url_error_hooks, sizeof(saved_on_url_error_hooks));
    int saved_has_only_tests = g_has_only_tests;
    int saved_has_only_groups = g_has_only_groups;
    int saved_has_only_imports = g_has_only_imports;
    char saved_only_req[512];
    char saved_only_import[512];
    strncpy(saved_only_req, g_only_req, sizeof(saved_only_req) - 1);
    saved_only_req[sizeof(saved_only_req) - 1] = '\0';
    strncpy(saved_only_import, g_only_import, sizeof(saved_only_import) - 1);
    saved_only_import[sizeof(saved_only_import) - 1] = '\0';
    g_before_all = NULL;
    g_before_each = NULL;
    g_after_each = NULL;
    g_after_all = NULL;
    g_on_error = NULL;
    g_on_assertion_error = NULL;
    g_on_network_error = NULL;
    g_before_url_hook_count = 0;
    g_after_url_hook_count = 0;
    g_on_url_error_hook_count = 0;
    g_has_only_tests = subtree_has_only_tests(root);
    g_has_only_groups = subtree_has_only_groups(root);
    g_has_only_imports = subtree_has_only_imports(root);

    for (ASTNode *scan = root; scan; scan = scan->right) {
        ASTNode *s = scan->left;
        if (s && s->type == AST_BEFORE_ALL) g_before_all = s->stmts;
        if (s && s->type == AST_BEFORE_EACH) g_before_each = s->stmts;
        if (s && s->type == AST_AFTER_EACH) g_after_each = s->stmts;
        if (s && s->type == AST_AFTER_ALL) g_after_all = s->stmts;
        if (s && s->type == AST_ON_ERROR) g_on_error = s->stmts;
        if (s && s->type == AST_ON_ASSERTION_ERROR) g_on_assertion_error = s->stmts;
        if (s && s->type == AST_ON_NETWORK_ERROR) g_on_network_error = s->stmts;
        if (s && s->type == AST_BEFORE_URL) add_url_hook(g_before_url_hooks, &g_before_url_hook_count, s->value, s->stmts);
        if (s && s->type == AST_AFTER_URL) add_url_hook(g_after_url_hooks, &g_after_url_hook_count, s->value, s->stmts);
        if (s && s->type == AST_ON_URL_ERROR) add_url_hook(g_on_url_error_hooks, &g_on_url_error_hook_count, s->value, s->stmts);
    }

    g_arg_count = o->arg_count;
    if (g_arg_count < 0) g_arg_count = 0;
    if (g_arg_count > 32) g_arg_count = 32;
    for (int i = 0; i < g_arg_count; i++) {
        strncpy(g_args[i], o->args[i], sizeof(g_args[i]) - 1);
        g_args[i][sizeof(g_args[i]) - 1] = '\0';
    }

    g_runtime_depth++;

    for (ASTNode *n = root; n; n = n->right) {
        ASTNode *stmt = n->left;
        if (!stmt || g_stop_requested) continue;

        if (!before_all_ran && g_before_all && (stmt->type == AST_TEST || stmt->type == AST_GROUP)) {
            Response hook_resp = {0, NULL, 0};
            int has_hook_resp = 0;
            StepRecord hook_steps[128];
            int hook_step_count = 0;
            int hook_step_idx = 0;
            for (ASTNode *b = g_before_all; b && !g_stop_requested; b = b->right) {
                exec_runtime_stmt(b, &hook_resp, &has_hook_resp, hook_steps, &hook_step_count, &hook_step_idx);
            }
            if (has_hook_resp) response_free(&hook_resp);
            before_all_ran = 1;
        }

        char alias_name[128] = {0};
        const char *store_name = NULL;

        if (g_import_only[0]) {
            if (!(stmt->type == AST_LET || stmt->type == AST_REQUEST_TEMPLATE)) continue;
            if (!stmt->is_export) continue;
            if (!import_alias_for(g_import_only, stmt->value, alias_name, sizeof alias_name)) continue;
            store_name = alias_name;
        }

        if (stmt->type == AST_LET) {
            if (strcmp(stmt->value, "__base_url__") == 0) {
                if (!g_base_url_locked) {
                    char tmp[512];
                    const char *v = resolve(stmt->left, tmp, sizeof tmp);
                    strncpy(g_base_url, v, sizeof(g_base_url) - 1);
                    g_base_url[sizeof(g_base_url) - 1] = '\0';
                    size_t bl2 = strlen(g_base_url);
                    if (bl2 > 0 && g_base_url[bl2 - 1] == '/') g_base_url[bl2 - 1] = '\0';
                }
            } else if (strcmp(stmt->value, "__timeout__") == 0) {
                if (!g_timeout_locked) {
                    char tmp[32];
                    const char *v = resolve(stmt->left, tmp, sizeof tmp);
                    g_timeout_ms = atoi(v);
                }
            } else if (strncmp(stmt->value, "__opt__", 7) == 0) {
                const char *k = stmt->value + 7;
                char tmp[512];
                const char *v = NULL;

                if (strcmp(k, "retry_backoff") == 0 && stmt->left &&
                    (stmt->left->type == AST_IDENT || stmt->left->type == AST_STRING)) {
                    v = stmt->left->value;
                } else {
                    v = resolve(stmt->left, tmp, sizeof tmp);
                }
                apply_runtime_option(k, v);
            } else {
                if (stmt->left && stmt->left->type == AST_SEND) {
                    Response r = exec_send(stmt->left);
                    var_set(store_name ? store_name : stmt->value, r.body ? r.body : "");
                    snapshot_last_response(&r);
                    g_total_req_ms += r.time_ms;
                    g_total_req_count++;
                    if (g_show_time) {
                        printf("%stime_ms%s %ld\n", cc(COL_DIM_RAW), cc(COL_RESET_RAW), r.time_ms);
                    }
                    response_free(&r);
                } else if (stmt->left && stmt->left->type == AST_OBJECT) {
                    store_object_var(store_name ? store_name : stmt->value, stmt->left);
                    var_set(store_name ? store_name : stmt->value, "__object__");
                } else {
                    char tmp[512];
                    const char *v = resolve(stmt->left, tmp, sizeof tmp);
                    var_set(store_name ? store_name : stmt->value, v);
                }
            }
        } else if (stmt->type == AST_REQUEST_TEMPLATE) {
            register_template(stmt, store_name ? store_name : stmt->value);
        } else if (stmt->type == AST_IF) {
            int branch = eval_condition(stmt->left);
            ASTNode *branch_head = branch ? stmt->stmts : stmt->alt;
            Response if_resp = {0, NULL, 0};
            int has_if_resp = 0;
            StepRecord if_steps[256];
            int if_step_count = 0;
            int if_step_idx = 0;

            int prev_stop_current_test = g_stop_current_test;
            g_stop_current_test = 0;

            for (ASTNode *inner = branch_head; inner && !g_stop_requested && !g_stop_current_test; inner = inner->right) {
                exec_runtime_stmt(inner, &if_resp, &has_if_resp, if_steps, &if_step_count, &if_step_idx);
            }

            g_stop_current_test = prev_stop_current_test;
            if (has_if_resp) response_free(&if_resp);
        } else if (stmt->type == AST_PRINT) {
            char tmp[512];
            const char *val = resolve(stmt->left, tmp, sizeof tmp);
            printf("%sprint%s %s\n", cc(COL_DIM_RAW), cc(COL_RESET_RAW), val ? val : "");
        } else if (stmt->type == AST_TIME_START) {
            if (timer_start(stmt->value) == 0) {
                printf("%stime_start%s %s\n", cc(COL_DIM_RAW), cc(COL_RESET_RAW), stmt->value);
            } else {
                printf("%sFAIL%s cannot start timer '%s'\n", cc(COL_RED_RAW), cc(COL_RESET_RAW), stmt->value);
            }
        } else if (stmt->type == AST_TIME_STOP) {
            long elapsed = 0;
            if (timer_stop(stmt->value, &elapsed) == 0) {
                char tbuf[64];
                snprintf(tbuf, sizeof(tbuf), "%ld", elapsed);
                var_set("last_timer_ms", tbuf);
                printf("%stime_stop%s %s -> %ldms\n", cc(COL_DIM_RAW), cc(COL_RESET_RAW), stmt->value, elapsed);
            } else {
                printf("%sFAIL%s timer '%s' is not running\n", cc(COL_RED_RAW), cc(COL_RESET_RAW), stmt->value);
            }
        } else if (stmt->type == AST_STOP_ALL) {
            g_stop_requested = 1;
            g_stopped_by_user = 1;
            if (stmt->skip_reason[0]) {
                printf("%sstop_all%s because \"%s\"\n", cc(COL_RED_RAW), cc(COL_RESET_RAW), stmt->skip_reason);
            } else {
                printf("%sstop_all%s requested\n", cc(COL_RED_RAW), cc(COL_RESET_RAW));
            }
            continue;
        } else if (stmt->type == AST_FAIL_IF) {
            int should_fail = eval_condition(stmt->left);
            if (should_fail) {
                total_fail++;
                g_assertion_failures++;
                trigger_error_hooks("assertion", g_last_req.url[0] ? g_last_req.url : NULL);
                g_stop_requested = 1;
                g_stopped_by_user = 1;
                if (stmt->skip_reason[0]) {
                    printf("%sFAIL%s fail_if because \"%s\"\n", cc(COL_RED_RAW), cc(COL_RESET_RAW), stmt->skip_reason);
                } else {
                    printf("%sFAIL%s fail_if condition matched\n", cc(COL_RED_RAW), cc(COL_RESET_RAW));
                }
            }
        } else if (stmt->type == AST_SLEEP) {
            char tbuf[64];
            const char *sv = resolve(stmt->left, tbuf, sizeof tbuf);
            int ms = atoi(sv ? sv : "0");
            if (ms < 0) ms = 0;
            bad_sleep_ms(ms);
            printf("%ssleep%s %dms\n", cc(COL_DIM_RAW), cc(COL_RESET_RAW), ms);
        } else if (stmt->type == AST_BEFORE_ALL) {
            g_before_all = stmt->stmts;
        } else if (stmt->type == AST_BEFORE_EACH) {
            g_before_each = stmt->stmts;
        } else if (stmt->type == AST_GROUP) {
            if (stmt->is_skip) {
                g_skipped_groups++;
                if (stmt->skip_reason[0]) {
                    printf("%s  - skipped group \"%s\" because \"%s\"%s\n",
                           cc(COL_YELLOW_RAW), stmt->value, stmt->skip_reason, cc(COL_RESET_RAW));
                } else {
                    printf("%s  - skipped group \"%s\"%s\n",
                           cc(COL_YELLOW_RAW), stmt->value, cc(COL_RESET_RAW));
                }
                continue;
            }

            if (g_has_only_tests) {
                int has_group_only_test = 0;
                for (ASTNode *gs = stmt->stmts; gs; gs = gs->right) {
                    if (gs->type == AST_TEST && gs->is_only) {
                        has_group_only_test = 1;
                        break;
                    }
                }
                if (!has_group_only_test) {
                    g_filtered_groups++;
                    continue;
                }
            } else if (g_has_only_groups && !stmt->is_only) {
                g_filtered_groups++;
                continue;
            }

            printf("\n%s%s▸ group \"%s\"%s\n", cc(COL_BOLD_RAW), cc(COL_DIM_RAW), stmt->value, cc(COL_RESET_RAW));

            ASTNode *group_before_all = NULL;
            int group_before_all_ran = 0;
            for (ASTNode *gs = stmt->stmts; gs; gs = gs->right) {
                if (gs->type == AST_BEFORE_ALL) {
                    group_before_all = gs->stmts;
                }
            }

            ASTNode *group_saved_before_all = g_before_all;
            ASTNode *group_saved_before_each = g_before_each;
            ASTNode *group_saved_after_each = g_after_each;
            ASTNode *group_saved_after_all = g_after_all;
            ASTNode *group_saved_on_error = g_on_error;
            ASTNode *group_saved_on_assertion_error = g_on_assertion_error;
            ASTNode *group_saved_on_network_error = g_on_network_error;
            UrlHook group_saved_before_url_hooks[64];
            UrlHook group_saved_after_url_hooks[64];
            UrlHook group_saved_on_url_error_hooks[64];
            int group_saved_before_url_hook_count = g_before_url_hook_count;
            int group_saved_after_url_hook_count = g_after_url_hook_count;
            int group_saved_on_url_error_hook_count = g_on_url_error_hook_count;

            memcpy(group_saved_before_url_hooks, g_before_url_hooks, sizeof(group_saved_before_url_hooks));
            memcpy(group_saved_after_url_hooks, g_after_url_hooks, sizeof(group_saved_after_url_hooks));
            memcpy(group_saved_on_url_error_hooks, g_on_url_error_hooks, sizeof(group_saved_on_url_error_hooks));

            g_before_all = group_before_all;

            for (ASTNode *gs = stmt->stmts; gs; gs = gs->right) {
                if (gs->type == AST_TEST) {
                    if (gs->is_skip) {
                        g_skipped_tests++;
                        if (gs->skip_reason[0]) {
                            printf("%s  - skipped test \"%s\" because \"%s\"%s\n",
                                   cc(COL_YELLOW_RAW), gs->value, gs->skip_reason, cc(COL_RESET_RAW));
                        } else {
                            printf("%s  - skipped test \"%s\"%s\n", cc(COL_YELLOW_RAW), gs->value, cc(COL_RESET_RAW));
                        }
                        continue;
                    }
                    if (g_has_only_tests && !gs->is_only) {
                        continue;
                    }

                    if (!group_before_all_ran && g_before_all && !g_stop_requested) {
                        Response hook_resp = {0, NULL, 0};
                        int has_hook_resp = 0;
                        StepRecord hook_steps[128];
                        int hook_step_count = 0;
                        int hook_step_idx = 0;
                        for (ASTNode *b = g_before_all; b && !g_stop_requested; b = b->right) {
                            exec_runtime_stmt(b, &hook_resp, &has_hook_resp, hook_steps, &hook_step_count, &hook_step_idx);
                        }
                        if (has_hook_resp) response_free(&hook_resp);
                        group_before_all_ran = 1;
                    }

                    exec_test(gs);
                    total_pass += g_pass;
                    total_fail += g_fail;
                } else if (gs->type == AST_LET) {
                    if (strcmp(gs->value, "__base_url__") == 0) {
                        if (!g_base_url_locked) {
                            char tmp[512];
                            const char *v = resolve(gs->left, tmp, sizeof tmp);
                            strncpy(g_base_url, v, sizeof(g_base_url) - 1);
                            g_base_url[sizeof(g_base_url) - 1] = '\0';
                            size_t bl2 = strlen(g_base_url);
                            if (bl2 > 0 && g_base_url[bl2 - 1] == '/') g_base_url[bl2 - 1] = '\0';
                        }
                    } else if (strcmp(gs->value, "__timeout__") == 0) {
                        if (!g_timeout_locked) {
                            char tmp[32];
                            const char *v = resolve(gs->left, tmp, sizeof tmp);
                            g_timeout_ms = atoi(v);
                        }
                    } else if (strncmp(gs->value, "__opt__", 7) == 0) {
                        const char *k = gs->value + 7;
                        char tmp[512];
                        const char *v = NULL;

                        if (strcmp(k, "retry_backoff") == 0 && gs->left &&
                            (gs->left->type == AST_IDENT || gs->left->type == AST_STRING)) {
                            v = gs->left->value;
                        } else {
                            v = resolve(gs->left, tmp, sizeof tmp);
                        }
                        apply_runtime_option(k, v);
                    } else {
                        if (gs->left && gs->left->type == AST_SEND) {
                            Response r = exec_send(gs->left);
                            var_set(gs->value, r.body ? r.body : "");
                            snapshot_last_response(&r);
                            g_total_req_ms += r.time_ms;
                            g_total_req_count++;
                            if (g_show_time) {
                                printf("%stime_ms%s %ld\n", cc(COL_DIM_RAW), cc(COL_RESET_RAW), r.time_ms);
                            }
                            response_free(&r);
                        } else if (gs->left && gs->left->type == AST_OBJECT) {
                            store_object_var(gs->value, gs->left);
                            var_set(gs->value, "__object__");
                        } else {
                            char tmp[512];
                            const char *v = resolve(gs->left, tmp, sizeof tmp);
                            var_set(gs->value, v);
                        }
                    }
                } else if (gs->type == AST_REQUEST_TEMPLATE) {
                    register_template(gs, gs->value);
                } else if (gs->type == AST_BEFORE_ALL) {
                    g_before_all = gs->stmts;
                } else if (gs->type == AST_BEFORE_EACH) {
                    g_before_each = gs->stmts;
                } else if (gs->type == AST_AFTER_EACH) {
                    g_after_each = gs->stmts;
                } else if (gs->type == AST_AFTER_ALL) {
                    g_after_all = gs->stmts;
                } else if (gs->type == AST_ON_ERROR) {
                    g_on_error = gs->stmts;
                } else if (gs->type == AST_ON_ASSERTION_ERROR) {
                    g_on_assertion_error = gs->stmts;
                } else if (gs->type == AST_ON_NETWORK_ERROR) {
                    g_on_network_error = gs->stmts;
                } else if (gs->type == AST_BEFORE_URL) {
                    add_url_hook(g_before_url_hooks, &g_before_url_hook_count, gs->value, gs->stmts);
                } else if (gs->type == AST_AFTER_URL) {
                    add_url_hook(g_after_url_hooks, &g_after_url_hook_count, gs->value, gs->stmts);
                } else if (gs->type == AST_ON_URL_ERROR) {
                    add_url_hook(g_on_url_error_hooks, &g_on_url_error_hook_count, gs->value, gs->stmts);
                } else if (gs->type == AST_IF) {
                    int branch = eval_condition(gs->left);
                    ASTNode *branch_head = branch ? gs->stmts : gs->alt;
                    Response if_resp = {0, NULL, 0};
                    int has_if_resp = 0;
                    StepRecord if_steps[256];
                    int if_step_count = 0;
                    int if_step_idx = 0;

                    int prev_stop_current_test = g_stop_current_test;
                    g_stop_current_test = 0;

                    for (ASTNode *inner = branch_head; inner && !g_stop_requested && !g_stop_current_test; inner = inner->right) {
                        exec_runtime_stmt(inner, &if_resp, &has_if_resp, if_steps, &if_step_count, &if_step_idx);
                    }

                    g_stop_current_test = prev_stop_current_test;
                    if (has_if_resp) response_free(&if_resp);
                } else if (gs->type == AST_PRINT) {
                    char tmp[512];
                    const char *val = resolve(gs->left, tmp, sizeof tmp);
                    printf("%sprint%s %s\n", cc(COL_DIM_RAW), cc(COL_RESET_RAW), val ? val : "");
                } else if (gs->type == AST_TIME_START) {
                    if (timer_start(gs->value) == 0) {
                        printf("%stime_start%s %s\n", cc(COL_DIM_RAW), cc(COL_RESET_RAW), gs->value);
                    } else {
                        printf("%sFAIL%s cannot start timer '%s'\n", cc(COL_RED_RAW), cc(COL_RESET_RAW), gs->value);
                    }
                } else if (gs->type == AST_TIME_STOP) {
                    long elapsed = 0;
                    if (timer_stop(gs->value, &elapsed) == 0) {
                        char tbuf[64];
                        snprintf(tbuf, sizeof(tbuf), "%ld", elapsed);
                        var_set("last_timer_ms", tbuf);
                        printf("%stime_stop%s %s -> %ldms\n", cc(COL_DIM_RAW), cc(COL_RESET_RAW), gs->value, elapsed);
                    } else {
                        printf("%sFAIL%s timer '%s' is not running\n", cc(COL_RED_RAW), cc(COL_RESET_RAW), gs->value);
                    }
                } else if (gs->type == AST_STOP_ALL) {
                    g_stop_requested = 1;
                    g_stopped_by_user = 1;
                    if (gs->skip_reason[0]) {
                        printf("%sstop_all%s because \"%s\"\n", cc(COL_RED_RAW), cc(COL_RESET_RAW), gs->skip_reason);
                    } else {
                        printf("%sstop_all%s requested\n", cc(COL_RED_RAW), cc(COL_RESET_RAW));
                    }
                    break;
                } else if (gs->type == AST_FAIL_IF) {
                    int should_fail = eval_condition(gs->left);
                    if (should_fail) {
                        total_fail++;
                        g_assertion_failures++;
                        trigger_error_hooks("assertion", g_last_req.url[0] ? g_last_req.url : NULL);
                        g_stop_requested = 1;
                        g_stopped_by_user = 1;
                        if (gs->skip_reason[0]) {
                            printf("%sFAIL%s fail_if because \"%s\"\n", cc(COL_RED_RAW), cc(COL_RESET_RAW), gs->skip_reason);
                        } else {
                            printf("%sFAIL%s fail_if condition matched\n", cc(COL_RED_RAW), cc(COL_RESET_RAW));
                        }
                        break;
                    }
                } else if (gs->type == AST_SLEEP) {
                    char tbuf[64];
                    const char *sv = resolve(gs->left, tbuf, sizeof tbuf);
                    int ms = atoi(sv ? sv : "0");
                    if (ms < 0) ms = 0;
                    bad_sleep_ms(ms);
                    printf("%ssleep%s %dms\n", cc(COL_DIM_RAW), cc(COL_RESET_RAW), ms);
                }
            }

            g_before_all = group_saved_before_all;
            g_before_each = group_saved_before_each;
            g_after_each = group_saved_after_each;
            g_after_all = group_saved_after_all;
            g_on_error = group_saved_on_error;
            g_on_assertion_error = group_saved_on_assertion_error;
            g_on_network_error = group_saved_on_network_error;
            g_before_url_hook_count = group_saved_before_url_hook_count;
            g_after_url_hook_count = group_saved_after_url_hook_count;
            g_on_url_error_hook_count = group_saved_on_url_error_hook_count;
            memcpy(g_before_url_hooks, group_saved_before_url_hooks, sizeof(g_before_url_hooks));
            memcpy(g_after_url_hooks, group_saved_after_url_hooks, sizeof(g_after_url_hooks));
            memcpy(g_on_url_error_hooks, group_saved_on_url_error_hooks, sizeof(g_on_url_error_hooks));
        } else if (stmt->type == AST_TEST) {
            if (stmt->is_skip) {
                g_skipped_tests++;
                if (stmt->skip_reason[0]) {
                    printf("%s  - skipped test \"%s\" because \"%s\"%s\n",
                           cc(COL_YELLOW_RAW), stmt->value, stmt->skip_reason, cc(COL_RESET_RAW));
                } else {
                    printf("%s  - skipped test \"%s\"%s\n", cc(COL_YELLOW_RAW), stmt->value, cc(COL_RESET_RAW));
                }
                continue;
            }
            if (g_has_only_tests && !stmt->is_only) {
                g_filtered_tests++;
                continue;
            }
            if (!g_has_only_tests && g_has_only_groups) {
                g_filtered_tests++;
                continue;
            }
            exec_test(stmt);
            total_pass += g_pass;
            total_fail += g_fail;
        } else if (stmt->type == AST_IMPORT) {
            if (g_has_only_imports && !stmt->is_only) {
                continue;
            }
            if (g_only_import[0] && !csv_contains_value(g_only_import, stmt->value)) {
                continue;
            }

            char resolved_import_path[1024];
            resolved_import_path[0] = '\0';
            resolve_import_path(stmt->value, resolved_import_path, sizeof(resolved_import_path));

            const char *import_path = stmt->value;
            FILE *imp = fopen(import_path, "r");
            if (!imp && resolved_import_path[0] && strcmp(resolved_import_path, stmt->value) != 0) {
                import_path = resolved_import_path;
                imp = fopen(import_path, "r");
            }

            if (!imp) {
                fprintf(stderr, "[bad] import: cannot open '%s'\n", stmt->value);
                total_fail++;
            } else {
                if (fseek(imp, 0, SEEK_END) != 0) {
                    fclose(imp);
                    fprintf(stderr, "[bad] import: failed to read '%s'\n", import_path);
                    total_fail++;
                } else {
                    long isz_long = ftell(imp);
                    if (isz_long < 0 || fseek(imp, 0, SEEK_SET) != 0) {
                        fclose(imp);
                        fprintf(stderr, "[bad] import: failed to read '%s'\n", import_path);
                        total_fail++;
                    } else {
                        size_t isz = (size_t)isz_long;
                        char *ibuf = malloc(isz + 1);
                        if (!ibuf) {
                            fclose(imp);
                            fprintf(stderr, "[bad] import: OOM reading '%s'\n", import_path);
                            total_fail++;
                        } else {
                            size_t nread = fread(ibuf, 1, isz, imp);
                            fclose(imp);
                            if (nread != isz) {
                                free(ibuf);
                                fprintf(stderr, "[bad] import: short read for '%s'\n", import_path);
                                total_fail++;
                                continue;
                            }
                            ibuf[isz] = '\0';

                            log_msg(LOG_INFO, "%s-> import \"%s\"%s\n", cc(COL_DIM_RAW), import_path, cc(COL_RESET_RAW));
                            lexer_init(ibuf);
                            ASTNode *iroot = parse();
                            RuntimeOptions child = *o;
                            strncpy(child.base_url, g_base_url, sizeof(child.base_url) - 1);
                            child.base_url[sizeof(child.base_url) - 1] = '\0';
                            child.timeout_ms = g_timeout_ms;
                            child.save_history = g_save;
                            child.flat_mode = g_flat;
                            child.table_mode = g_table;
                            child.print_request = g_print_request;
                            child.print_response = g_print_response;
                            child.use_color = g_use_color;
                            child.fail_fast = g_fail_fast;
                            child.strict_runtime_errors = g_strict_runtime_errors;
                            child.save_steps = g_save_steps;
                            child.remember_token = g_remember_token;
                            child.show_time = g_show_time;
                            child.show_timestamp = g_show_timestamp;
                            child.json_view = g_json_view;
                            child.json_pretty = g_json_pretty;
                            strncpy(child.log_level, (g_log_level == LOG_DEBUG) ? "debug" : (g_log_level == LOG_ERROR ? "error" : "info"), sizeof(child.log_level) - 1);
                            child.log_level[sizeof(child.log_level) - 1] = '\0';
                            strncpy(child.history_dir, g_history_dir, sizeof(child.history_dir) - 1);
                            child.history_dir[sizeof(child.history_dir) - 1] = '\0';
                            strncpy(child.history_file, g_history_file, sizeof(child.history_file) - 1);
                            child.history_file[sizeof(child.history_file) - 1] = '\0';
                            strncpy(child.history_format, g_history_format, sizeof(child.history_format) - 1);
                            child.history_format[sizeof(child.history_format) - 1] = '\0';
                            strncpy(child.history_mode, g_history_mode, sizeof(child.history_mode) - 1);
                            child.history_mode[sizeof(child.history_mode) - 1] = '\0';
                            strncpy(child.history_methods, g_history_methods, sizeof(child.history_methods) - 1);
                            child.history_methods[sizeof(child.history_methods) - 1] = '\0';
                            strncpy(child.history_exclude_methods, g_history_exclude_methods, sizeof(child.history_exclude_methods) - 1);
                            child.history_exclude_methods[sizeof(child.history_exclude_methods) - 1] = '\0';
                            child.history_only_failed = g_history_only_failed;
                            child.history_include_headers = g_history_include_headers;
                            child.history_include_request_body = g_history_include_request_body;
                            child.history_include_response_body = g_history_include_response_body;
                            child.history_max_body_bytes = g_history_max_body_bytes;
                            strncpy(child.source_file, import_path, sizeof(child.source_file) - 1);
                            child.source_file[sizeof(child.source_file) - 1] = '\0';
                            if (stmt->extra && stmt->extra->value[0]) {
                                strncpy(child.import_only, stmt->extra->value, sizeof(child.import_only) - 1);
                                child.import_only[sizeof(child.import_only) - 1] = '\0';
                            } else {
                                child.import_only[0] = '\0';
                            }

                    int saved_verbose = g_verbose;
                    int saved_save = g_save;
                    int saved_flat = g_flat;
                    int saved_table = g_table;
                    int saved_print_request = g_print_request;
                    int saved_print_response = g_print_response;
                    int saved_use_color = g_use_color;
                    int saved_fail_fast = g_fail_fast;
                    int saved_strict_runtime_errors = g_strict_runtime_errors;
                    int saved_save_steps = g_save_steps;
                    int saved_json_view = g_json_view;
                    int saved_json_pretty = g_json_pretty;
                    int saved_remember_token = g_remember_token;
                    int saved_show_time = g_show_time;
                    int saved_show_timestamp = g_show_timestamp;
                    int saved_log_level = g_log_level;
                    int saved_base_url_locked = g_base_url_locked;
                    int saved_timeout_locked = g_timeout_locked;
                    int saved_stop_requested = g_stop_requested;
                    int saved_stopped_by_user = g_stopped_by_user;
                    int saved_runtime_depth = g_runtime_depth;
                    int saved_timeout_ms = g_timeout_ms;

                    char saved_base_url[512];
                    char saved_history_dir[256];
                    char saved_history_file[512];
                    char saved_history_format[16];
                    char saved_history_mode[16];
                    char saved_history_methods[128];
                    char saved_history_exclude_methods[128];
                    int saved_history_only_failed;
                    int saved_history_include_headers;
                    int saved_history_include_request_body;
                    int saved_history_include_response_body;
                    int saved_history_max_body_bytes;
                    char saved_source_file[512];
                    char saved_import_only[512];
                    char saved_only_req_inner[512];
                    char saved_only_import_inner[512];
                    ASTNode *saved_child_before_each = g_before_each;
                    ASTNode *saved_child_after_each = g_after_each;
                    ASTNode *saved_child_after_all = g_after_all;
                    ASTNode *saved_child_before_all = g_before_all;
                    int saved_child_has_only_tests = g_has_only_tests;
                    int saved_child_has_only_groups = g_has_only_groups;
                    int saved_child_has_only_imports = g_has_only_imports;

                    strncpy(saved_base_url, g_base_url, sizeof(saved_base_url) - 1);
                    saved_base_url[sizeof(saved_base_url) - 1] = '\0';
                    strncpy(saved_history_dir, g_history_dir, sizeof(saved_history_dir) - 1);
                    saved_history_dir[sizeof(saved_history_dir) - 1] = '\0';
                    strncpy(saved_history_file, g_history_file, sizeof(saved_history_file) - 1);
                    saved_history_file[sizeof(saved_history_file) - 1] = '\0';
                    strncpy(saved_history_format, g_history_format, sizeof(saved_history_format) - 1);
                    saved_history_format[sizeof(saved_history_format) - 1] = '\0';
                    strncpy(saved_history_mode, g_history_mode, sizeof(saved_history_mode) - 1);
                    saved_history_mode[sizeof(saved_history_mode) - 1] = '\0';
                    strncpy(saved_history_methods, g_history_methods, sizeof(saved_history_methods) - 1);
                    saved_history_methods[sizeof(saved_history_methods) - 1] = '\0';
                    strncpy(saved_history_exclude_methods, g_history_exclude_methods, sizeof(saved_history_exclude_methods) - 1);
                    saved_history_exclude_methods[sizeof(saved_history_exclude_methods) - 1] = '\0';
                    saved_history_only_failed = g_history_only_failed;
                    saved_history_include_headers = g_history_include_headers;
                    saved_history_include_request_body = g_history_include_request_body;
                    saved_history_include_response_body = g_history_include_response_body;
                    saved_history_max_body_bytes = g_history_max_body_bytes;
                    strncpy(saved_source_file, g_source_file, sizeof(saved_source_file) - 1);
                    saved_source_file[sizeof(saved_source_file) - 1] = '\0';
                    strncpy(saved_import_only, g_import_only, sizeof(saved_import_only) - 1);
                    saved_import_only[sizeof(saved_import_only) - 1] = '\0';
                    strncpy(saved_only_req_inner, g_only_req, sizeof(saved_only_req_inner) - 1);
                    saved_only_req_inner[sizeof(saved_only_req_inner) - 1] = '\0';
                    strncpy(saved_only_import_inner, g_only_import, sizeof(saved_only_import_inner) - 1);
                    saved_only_import_inner[sizeof(saved_only_import_inner) - 1] = '\0';

                    total_fail += runtime_exec(iroot, &child);

                    g_verbose = saved_verbose;
                    g_save = saved_save;
                    g_flat = saved_flat;
                    g_table = saved_table;
                    g_print_request = saved_print_request;
                    g_print_response = saved_print_response;
                    g_use_color = saved_use_color;
                    g_fail_fast = saved_fail_fast;
                    g_strict_runtime_errors = saved_strict_runtime_errors;
                    g_save_steps = saved_save_steps;
                    g_json_view = saved_json_view;
                    g_json_pretty = saved_json_pretty;
                    g_remember_token = saved_remember_token;
                    g_show_time = saved_show_time;
                    g_show_timestamp = saved_show_timestamp;
                    g_log_level = saved_log_level;
                    g_base_url_locked = saved_base_url_locked;
                    g_timeout_locked = saved_timeout_locked;
                    g_stop_requested = saved_stop_requested;
                    g_stopped_by_user = saved_stopped_by_user;
                    g_runtime_depth = saved_runtime_depth;
                    g_timeout_ms = saved_timeout_ms;

                    strncpy(g_base_url, saved_base_url, sizeof(g_base_url) - 1);
                    g_base_url[sizeof(g_base_url) - 1] = '\0';
                    strncpy(g_history_dir, saved_history_dir, sizeof(g_history_dir) - 1);
                    g_history_dir[sizeof(g_history_dir) - 1] = '\0';
                    strncpy(g_history_file, saved_history_file, sizeof(g_history_file) - 1);
                    g_history_file[sizeof(g_history_file) - 1] = '\0';
                    strncpy(g_history_format, saved_history_format, sizeof(g_history_format) - 1);
                    g_history_format[sizeof(g_history_format) - 1] = '\0';
                    strncpy(g_history_mode, saved_history_mode, sizeof(g_history_mode) - 1);
                    g_history_mode[sizeof(g_history_mode) - 1] = '\0';
                    strncpy(g_history_methods, saved_history_methods, sizeof(g_history_methods) - 1);
                    g_history_methods[sizeof(g_history_methods) - 1] = '\0';
                    strncpy(g_history_exclude_methods, saved_history_exclude_methods, sizeof(g_history_exclude_methods) - 1);
                    g_history_exclude_methods[sizeof(g_history_exclude_methods) - 1] = '\0';
                    g_history_only_failed = saved_history_only_failed;
                    g_history_include_headers = saved_history_include_headers;
                    g_history_include_request_body = saved_history_include_request_body;
                    g_history_include_response_body = saved_history_include_response_body;
                    g_history_max_body_bytes = saved_history_max_body_bytes;
                    strncpy(g_source_file, saved_source_file, sizeof(g_source_file) - 1);
                    g_source_file[sizeof(g_source_file) - 1] = '\0';
                    strncpy(g_import_only, saved_import_only, sizeof(g_import_only) - 1);
                    g_import_only[sizeof(g_import_only) - 1] = '\0';
                    strncpy(g_only_req, saved_only_req_inner, sizeof(g_only_req) - 1);
                    g_only_req[sizeof(g_only_req) - 1] = '\0';
                    strncpy(g_only_import, saved_only_import_inner, sizeof(g_only_import) - 1);
                    g_only_import[sizeof(g_only_import) - 1] = '\0';
                    g_before_each = saved_child_before_each;
                    g_after_each = saved_child_after_each;
                    g_after_all = saved_child_after_all;
                    g_before_all = saved_child_before_all;
                    g_has_only_tests = saved_child_has_only_tests;
                    g_has_only_groups = saved_child_has_only_groups;
                    g_has_only_imports = saved_child_has_only_imports;
                            ast_free(iroot);
                            free(ibuf);
                        }
                    }
                }
            }
        }
    }

    if (g_after_all && !g_stop_requested) {
        Response hook_resp = {0, NULL, 0};
        int has_hook_resp = 0;
        StepRecord hook_steps[64];
        int hook_step_count = 0;
        int hook_step_idx = 0;

        for (ASTNode *a = g_after_all; a && !g_stop_requested; a = a->right) {
            exec_runtime_stmt(a, &hook_resp, &has_hook_resp, hook_steps, &hook_step_count, &hook_step_idx);
        }
        if (has_hook_resp) response_free(&hook_resp);
    }

    g_runtime_depth--;
    int strict_soft_failures = (g_runtime_depth == 0 && g_strict_runtime_errors) ? g_soft_runtime_errors : 0;
    int effective_total_fail = total_fail + strict_soft_failures;
    if (g_runtime_depth == 0) {
        printf("\n%s%s────────────────────────────────%s\n", cc(COL_BOLD_RAW), cc(COL_DIM_RAW), cc(COL_RESET_RAW));
        if (effective_total_fail == 0) {
            printf("%s%sAll %d assertions passed%s\n", cc(COL_GREEN_RAW), cc(COL_BOLD_RAW), total_pass, cc(COL_RESET_RAW));
        } else {
            printf("%s%s%d failed, %d passed%s\n", cc(COL_RED_RAW), cc(COL_BOLD_RAW), effective_total_fail, total_pass, cc(COL_RESET_RAW));
        }
        if (g_skipped_tests || g_skipped_groups || g_filtered_tests || g_filtered_groups) {
            printf("%sSkipped:%s tests=%d groups=%d\n", cc(COL_YELLOW_RAW), cc(COL_RESET_RAW), g_skipped_tests, g_skipped_groups);
            printf("%sFiltered:%s tests=%d groups=%d\n", cc(COL_DIM_RAW), cc(COL_RESET_RAW), g_filtered_tests, g_filtered_groups);
        }
        if (g_stop_requested) {
            if (g_stopped_by_user) {
                printf("%sStopped early due to stop_all%s\n", cc(COL_YELLOW_RAW), cc(COL_RESET_RAW));
            } else {
                printf("%sStopped early due to --fail-fast%s\n", cc(COL_YELLOW_RAW), cc(COL_RESET_RAW));
            }
        }
        if (g_assertion_failures || g_network_failures) {
            printf("%sFailure breakdown:%s assertions=%d network=%d\n",
                   cc(COL_DIM_RAW), cc(COL_RESET_RAW), g_assertion_failures, g_network_failures);
        }
        if (g_soft_runtime_errors > 0) {
            printf("%sSoft errors:%s %d%s\n",
                   cc(COL_DIM_RAW), cc(COL_RESET_RAW), g_soft_runtime_errors,
                   g_strict_runtime_errors ? " (counted as failures)" : " (not counted as assertion failures)");
        }
        if (g_zero_assert_tests > 0) {
            printf("%sWarnings:%s %d test(s) ran with zero assertions\n",
                   cc(COL_YELLOW_RAW), cc(COL_RESET_RAW), g_zero_assert_tests);
        }
        if (g_show_time && g_total_req_count > 0) {
            long avg = g_total_req_ms / g_total_req_count;
            printf("%sRequest timing:%s count=%d total=%ldms avg=%ldms\n",
                   cc(COL_DIM_RAW), cc(COL_RESET_RAW), g_total_req_count, g_total_req_ms, avg);
        }
        printf("%s%s────────────────────────────────%s\n", cc(COL_BOLD_RAW), cc(COL_DIM_RAW), cc(COL_RESET_RAW));
    }

    g_before_all = saved_before_all;
    g_before_each = saved_before_each;
    g_after_each = saved_after_each;
    g_after_all = saved_after_all;
    g_on_error = saved_on_error;
    g_on_assertion_error = saved_on_assertion_error;
    g_on_network_error = saved_on_network_error;
    g_before_url_hook_count = saved_before_url_hook_count;
    g_after_url_hook_count = saved_after_url_hook_count;
    g_on_url_error_hook_count = saved_on_url_error_hook_count;
    memcpy(g_before_url_hooks, saved_before_url_hooks, sizeof(g_before_url_hooks));
    memcpy(g_after_url_hooks, saved_after_url_hooks, sizeof(g_after_url_hooks));
    memcpy(g_on_url_error_hooks, saved_on_url_error_hooks, sizeof(g_on_url_error_hooks));
    g_has_only_tests = saved_has_only_tests;
    g_has_only_groups = saved_has_only_groups;
    g_has_only_imports = saved_has_only_imports;
    strncpy(g_only_req, saved_only_req, sizeof(g_only_req) - 1);
    g_only_req[sizeof(g_only_req) - 1] = '\0';
    strncpy(g_only_import, saved_only_import, sizeof(g_only_import) - 1);
    g_only_import[sizeof(g_only_import) - 1] = '\0';

    return (g_runtime_depth == 0) ? effective_total_fail : total_fail;
}
