/*
 * vars.c  — simple string variable store + config loader
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bad.h"

/* ─── variable store ─────────────────────────────────────────── */

#define MAX_VARS 256

static struct { char name[128]; char value[512]; } vars[MAX_VARS];
static int var_count = 0;

static int parse_bool_value(const char *value) {
    if (!value) return 0;
    return strcmp(value, "true") == 0 ||
           strcmp(value, "1") == 0 ||
           strcmp(value, "yes") == 0 ||
           strcmp(value, "on") == 0;
}

void var_set(const char *name, const char *value) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0) {
            strncpy(vars[i].value, value, 511);
            vars[i].value[511] = '\0';
            return;
        }
    }
    if (var_count >= MAX_VARS) {
        fprintf(stderr, "[bad] variable store full\n");
        return;
    }
    strncpy(vars[var_count].name,  name,  127);
    strncpy(vars[var_count].value, value, 511);
    vars[var_count].name[127]  = '\0';
    vars[var_count].value[511] = '\0';
    var_count++;
}

const char *var_get(const char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0)
            return vars[i].value;
    }
    return NULL;
}

void var_clear(void) { var_count = 0; }

/* ─── config ─────────────────────────────────────────────────── */

BadConfig config_default(void) {
    BadConfig c;
    memset(&c, 0, sizeof c);
    c.timeout_ms    = 10000;
    c.pretty_output = 1;
    c.save_history  = 0;
    c.print_request = 0;
    c.print_response = 0;
    c.use_color = 1;
    c.fail_fast = 0;
    c.strict_runtime_errors = 0;
    c.save_steps = 0;
    c.remember_token = 0;
    c.show_time = 0;
    c.show_timestamp = 0;
    c.json_view = 0;
    c.json_pretty = 0;
    strncpy(c.log_level, "info", sizeof(c.log_level) - 1);
    strncpy(c.history_dir, ".bad-history", sizeof(c.history_dir) - 1);
    strncpy(c.history_format, "jsonl", sizeof(c.history_format) - 1);
    strncpy(c.history_mode, "all", sizeof(c.history_mode) - 1);
    c.history_only_failed = 0;
    c.history_include_headers = 1;
    c.history_include_request_body = 1;
    c.history_include_response_body = 1;
    c.history_max_body_bytes = 0;
    return c;
}

/* Tiny .badrc reader — JSON-ish: "key": value lines */
static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '\0') return s;
    char *e = s + strlen(s) - 1;
    while (e > s && (*e == ' ' || *e == '\t' || *e == '\r' || *e == '\n' ||
                     *e == ',' || *e == '"')) *e-- = '\0';
    return s;
}

BadConfig config_load(const char *path) {
    BadConfig c = config_default();
    FILE *f = fopen(path, "r");
    if (!f) return c;

    char line[512];
    while (fgets(line, sizeof line, f)) {
        /* find colon */
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        char *key = trim(line);
        char *val = trim(colon + 1);

        if (!key[0]) continue;

        /* strip leading quote from key */
        if (*key == '"') key++;

        size_t key_len = strlen(key);
        if (key_len == 0) continue;
        if (key[key_len - 1] == '"') key[key_len - 1] = '\0';
        if (!key[0]) continue;

        /* strip quotes from val */
        if (*val == '"') val++;

        size_t val_len = strlen(val);
        if (val_len > 0 && val[val_len - 1] == '"') val[val_len - 1] = '\0';

        if (strcmp(key, "base_url") == 0) {
            strncpy(c.base_url, val, 511);
        } else if (strcmp(key, "timeout") == 0) {
            c.timeout_ms = atoi(val);
        } else if (strcmp(key, "pretty_output") == 0) {
            c.pretty_output = parse_bool_value(val);
        } else if (strcmp(key, "save_history") == 0) {
            c.save_history = parse_bool_value(val);
        } else if (strcmp(key, "print_request") == 0) {
            c.print_request = parse_bool_value(val);
        } else if (strcmp(key, "print_response") == 0) {
            c.print_response = parse_bool_value(val);
        } else if (strcmp(key, "use_color") == 0) {
            c.use_color = parse_bool_value(val);
        } else if (strcmp(key, "fail_fast") == 0) {
            c.fail_fast = parse_bool_value(val);
        } else if (strcmp(key, "strict_runtime_errors") == 0) {
            c.strict_runtime_errors = parse_bool_value(val);
        } else if (strcmp(key, "save_steps") == 0) {
            c.save_steps = parse_bool_value(val);
        } else if (strcmp(key, "remember_token") == 0) {
            c.remember_token = parse_bool_value(val);
        } else if (strcmp(key, "show_time") == 0) {
            c.show_time = parse_bool_value(val);
        } else if (strcmp(key, "show_timestamp") == 0) {
            c.show_timestamp = parse_bool_value(val);
        } else if (strcmp(key, "json_view") == 0) {
            c.json_view = parse_bool_value(val);
        } else if (strcmp(key, "json_pretty") == 0) {
            c.json_pretty = parse_bool_value(val);
        } else if (strcmp(key, "log_level") == 0) {
            strncpy(c.log_level, val, sizeof(c.log_level) - 1);
            c.log_level[sizeof(c.log_level) - 1] = '\0';
        } else if (strcmp(key, "history_dir") == 0) {
            strncpy(c.history_dir, val, sizeof(c.history_dir) - 1);
            c.history_dir[sizeof(c.history_dir) - 1] = '\0';
        } else if (strcmp(key, "history_file") == 0) {
            strncpy(c.history_file, val, sizeof(c.history_file) - 1);
            c.history_file[sizeof(c.history_file) - 1] = '\0';
        } else if (strcmp(key, "history_format") == 0) {
            strncpy(c.history_format, val, sizeof(c.history_format) - 1);
            c.history_format[sizeof(c.history_format) - 1] = '\0';
        } else if (strcmp(key, "history_mode") == 0) {
            strncpy(c.history_mode, val, sizeof(c.history_mode) - 1);
            c.history_mode[sizeof(c.history_mode) - 1] = '\0';
        } else if (strcmp(key, "history_methods") == 0) {
            strncpy(c.history_methods, val, sizeof(c.history_methods) - 1);
            c.history_methods[sizeof(c.history_methods) - 1] = '\0';
        } else if (strcmp(key, "history_exclude_methods") == 0) {
            strncpy(c.history_exclude_methods, val, sizeof(c.history_exclude_methods) - 1);
            c.history_exclude_methods[sizeof(c.history_exclude_methods) - 1] = '\0';
        } else if (strcmp(key, "history_only_failed") == 0) {
            c.history_only_failed = parse_bool_value(val);
        } else if (strcmp(key, "history_include_headers") == 0) {
            c.history_include_headers = parse_bool_value(val);
        } else if (strcmp(key, "history_include_request_body") == 0) {
            c.history_include_request_body = parse_bool_value(val);
        } else if (strcmp(key, "history_include_response_body") == 0) {
            c.history_include_response_body = parse_bool_value(val);
        } else if (strcmp(key, "history_max_body_bytes") == 0) {
            c.history_max_body_bytes = atoi(val);
        }
    }
    fclose(f);
    return c;
}
