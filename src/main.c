/*
 * main.c  —  Bad Language CLI entry point
 *
 * Usage:
 *   ./bad test.bad
 *   ./bad test.bad --verbose
 *   ./bad test.bad --save
 *   ./bad test.bad --flat
 *   ./bad test.bad --table
 *   ./bad test.bad --base https://api.example.com
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bad.h"
#include "bad_platform.h"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "[bad] cannot open '%s'\n", path);
        exit(1);
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        fprintf(stderr, "[bad] failed to read '%s'\n", path);
        exit(1);
    }

    long sz_long = ftell(f);
    if (sz_long < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        fprintf(stderr, "[bad] failed to read '%s'\n", path);
        exit(1);
    }

    size_t sz = (size_t)sz_long;
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); fprintf(stderr, "[bad] OOM\n"); exit(1); }

    size_t nread = fread(buf, 1, sz, f);
    if (nread != sz) {
        fclose(f);
        free(buf);
        fprintf(stderr, "[bad] short read '%s'\n", path);
        exit(1);
    }

    buf[nread] = '\0';
    fclose(f);
    return buf;
}

static void print_usage(void) {
    printf(
        "\n"
        "  bad — API testing DSL\n"
        "\n"
        "  Usage: bad <file.bad> [options]\n"
        "         bad <file.bad> [options] -- [arg0 arg1 ...]\n"
        "\n"
        "  Options:\n"
        "    --verbose             show detailed execution output\n"
        "    --flat                output flat key=value format\n"
        "    --table               output table format (for arrays)\n"
        "    --save                save request/response history\n"
        "    --save-dir <path>     set history directory (default .bad-history)\n"
        "    --save-file <path>    append all history records to one JSONL file\n"
        "    --save-steps          save per-test execution steps in history\n"
        "    --save-mode <mode>    all | per-file | per-test | off\n"
        "    --save-methods <csv>  save only listed HTTP methods (e.g. GET,POST)\n"
        "    --save-exclude-methods <csv>  skip listed HTTP methods\n"
        "    --save-only-failed    save history only for failed tests\n"
        "    --save-headers        include request headers in history\n"
        "    --no-save-headers     omit request headers in history\n"
        "    --save-request-body   include request body in history\n"
        "    --no-save-request-body omit request body in history\n"
        "    --save-response-body  include response body in history\n"
        "    --no-save-response-body omit response body in history\n"
        "    --save-max-body-bytes <n>  truncate saved bodies to N bytes (0 = unlimited)\n"
        "    --history-format <f>  jsonl (default)\n"
        "    --base <url>          override base URL\n"
        "    --timeout <ms>        request timeout in ms\n"
        "    --print-request       print full request (method/url/headers/body)\n"
        "    --print-response      print full response body\n"
        "    --show-time           print request duration in ms\n"
        "    --show-timestamp      print request timestamp before each send\n"
        "    --json-view           force structured JSON tree output\n"
        "    --json-pretty         print raw response as pretty JSON\n"
        "    --remember-token      auto-capture token and re-use in Authorization header\n"
        "    --full-trace          enable both request + response printing\n"
        "    --log-level <level>   error | info | debug\n"
        "    --color <mode>        auto | always | never\n"
        "    --no-color            disable ANSI colors\n"
        "    --config <path>       load config file (default .badrc)\n"
        "    --fail-fast           stop on first failed assertion\n"
        "    --strict-runtime-errors  treat runtime soft errors as failures\n"
        "    --no-strict-runtime-errors disable strict runtime soft-error failures\n"
        "\n"
        "  Example:\n"
        "    bad tests/login.bad --verbose --print-request\n"
        "    bad tests/api.bad --base https://staging.api.com --save --save-dir .history\n"
        "\n"
    );
}

static void normalize_spaces(char *s) {
    if (!s || !*s) return;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') {
        memmove(s, s + 1, strlen(s));
    }
    size_t len = strlen(s);
    while (len > 0 &&
           (s[len - 1] == ' ' || s[len - 1] == '\t' ||
            s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

static int is_flag(const char *s) {
    return s && s[0] == '-';
}

RuntimeOptions runtime_options_default(void) {
    RuntimeOptions o;
    memset(&o, 0, sizeof o);
    o.timeout_ms = 10000;
    o.use_color = 1;
    o.json_pretty = 0;
    o.strict_runtime_errors = 0;
    o.history_only_failed = 0;
    o.history_include_headers = 1;
    o.history_include_request_body = 1;
    o.history_include_response_body = 1;
    o.history_max_body_bytes = 0;
    strncpy(o.log_level, "info", sizeof(o.log_level) - 1);
    strncpy(o.history_dir, ".bad-history", sizeof(o.history_dir) - 1);
    strncpy(o.history_format, "jsonl", sizeof(o.history_format) - 1);
    strncpy(o.history_mode, "all", sizeof(o.history_mode) - 1);
    return o;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char *config_path = ".badrc";
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--config") == 0 || strcmp(argv[i], "-c") == 0) && i + 1 < argc) {
            config_path = argv[i + 1];
            break;
        }
    }

    BadConfig cfg = config_load(config_path);
    RuntimeOptions opts = runtime_options_default();

    if (cfg.base_url[0]) strncpy(opts.base_url, cfg.base_url, sizeof(opts.base_url) - 1);
    opts.timeout_ms = cfg.timeout_ms > 0 ? cfg.timeout_ms : opts.timeout_ms;
    opts.save_history = cfg.save_history;
    opts.print_request = cfg.print_request;
    opts.print_response = cfg.print_response;
    opts.use_color = cfg.use_color;
    opts.fail_fast = cfg.fail_fast;
    opts.strict_runtime_errors = cfg.strict_runtime_errors;
    opts.save_steps = cfg.save_steps;
    opts.remember_token = cfg.remember_token;
    opts.show_time = cfg.show_time;
    opts.show_timestamp = cfg.show_timestamp;
    opts.json_view = cfg.json_view;
    opts.json_pretty = cfg.json_pretty;
    if (cfg.log_level[0]) strncpy(opts.log_level, cfg.log_level, sizeof(opts.log_level) - 1);
    if (cfg.history_dir[0]) strncpy(opts.history_dir, cfg.history_dir, sizeof(opts.history_dir) - 1);
    if (cfg.history_file[0]) strncpy(opts.history_file, cfg.history_file, sizeof(opts.history_file) - 1);
    if (cfg.history_format[0]) strncpy(opts.history_format, cfg.history_format, sizeof(opts.history_format) - 1);
    if (cfg.history_mode[0]) {
        strncpy(opts.history_mode, cfg.history_mode, sizeof(opts.history_mode) - 1);
        opts.history_mode[sizeof(opts.history_mode) - 1] = '\0';
    }
    if (cfg.history_methods[0]) {
        strncpy(opts.history_methods, cfg.history_methods, sizeof(opts.history_methods) - 1);
        opts.history_methods[sizeof(opts.history_methods) - 1] = '\0';
    }
    if (cfg.history_exclude_methods[0]) {
        strncpy(opts.history_exclude_methods, cfg.history_exclude_methods, sizeof(opts.history_exclude_methods) - 1);
        opts.history_exclude_methods[sizeof(opts.history_exclude_methods) - 1] = '\0';
    }
    opts.history_only_failed = cfg.history_only_failed;
    opts.history_include_headers = cfg.history_include_headers;
    opts.history_include_request_body = cfg.history_include_request_body;
    opts.history_include_response_body = cfg.history_include_response_body;
    opts.history_max_body_bytes = cfg.history_max_body_bytes;

    const char *filename = NULL;

    int passthrough_args = 0;
    for (int i = 1; i < argc; i++) {
        if (!passthrough_args && strcmp(argv[i], "--") == 0) {
            passthrough_args = 1;
            continue;
        }

        if (!passthrough_args && is_flag(argv[i])) {
            if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0)
                opts.verbose = 1;
            else if (strcmp(argv[i], "--save") == 0 || strcmp(argv[i], "-s") == 0)
                opts.save_history = 1;
            else if (strcmp(argv[i], "--flat") == 0)
                opts.flat_mode = 1;
            else if (strcmp(argv[i], "--table") == 0)
                opts.table_mode = 1;
            else if (strcmp(argv[i], "--base") == 0 && i + 1 < argc)
                strncpy(opts.base_url, argv[++i], sizeof(opts.base_url) - 1), opts.base_url_overridden = 1;
            else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc)
                opts.timeout_ms = atoi(argv[++i]), opts.timeout_overridden = 1;
            else if ((strcmp(argv[i], "--config") == 0 || strcmp(argv[i], "-c") == 0) && i + 1 < argc)
                i++;
            else if (strcmp(argv[i], "--save-dir") == 0 && i + 1 < argc)
                strncpy(opts.history_dir, argv[++i], sizeof(opts.history_dir) - 1);
            else if (strcmp(argv[i], "--save-file") == 0 && i + 1 < argc)
                strncpy(opts.history_file, argv[++i], sizeof(opts.history_file) - 1);
            else if (strcmp(argv[i], "--save-steps") == 0)
                opts.save_steps = 1;
            else if (strcmp(argv[i], "--save-mode") == 0 && i + 1 < argc) {
                strncpy(opts.history_mode, argv[++i], sizeof(opts.history_mode) - 1);
                opts.history_mode[sizeof(opts.history_mode) - 1] = '\0';
            }
            else if (strcmp(argv[i], "--save-methods") == 0 && i + 1 < argc) {
                strncpy(opts.history_methods, argv[++i], sizeof(opts.history_methods) - 1);
                opts.history_methods[sizeof(opts.history_methods) - 1] = '\0';
            }
            else if (strcmp(argv[i], "--save-exclude-methods") == 0 && i + 1 < argc) {
                strncpy(opts.history_exclude_methods, argv[++i], sizeof(opts.history_exclude_methods) - 1);
                opts.history_exclude_methods[sizeof(opts.history_exclude_methods) - 1] = '\0';
            }
            else if (strcmp(argv[i], "--save-only-failed") == 0 || strcmp(argv[i], "--save-only-failures") == 0)
                opts.history_only_failed = 1;
            else if (strcmp(argv[i], "--save-headers") == 0)
                opts.history_include_headers = 1;
            else if (strcmp(argv[i], "--no-save-headers") == 0)
                opts.history_include_headers = 0;
            else if (strcmp(argv[i], "--save-request-body") == 0)
                opts.history_include_request_body = 1;
            else if (strcmp(argv[i], "--no-save-request-body") == 0)
                opts.history_include_request_body = 0;
            else if (strcmp(argv[i], "--save-response-body") == 0)
                opts.history_include_response_body = 1;
            else if (strcmp(argv[i], "--no-save-response-body") == 0)
                opts.history_include_response_body = 0;
            else if (strcmp(argv[i], "--save-max-body-bytes") == 0 && i + 1 < argc)
                opts.history_max_body_bytes = atoi(argv[++i]);
            else if (strcmp(argv[i], "--history-format") == 0 && i + 1 < argc)
                strncpy(opts.history_format, argv[++i], sizeof(opts.history_format) - 1);
            else if (strcmp(argv[i], "--print-request") == 0)
                opts.print_request = 1;
            else if (strcmp(argv[i], "--print-response") == 0)
                opts.print_response = 1;
            else if (strcmp(argv[i], "--show-time") == 0 || strcmp(argv[i], "--timing") == 0)
                opts.show_time = 1;
            else if (strcmp(argv[i], "--show-timestamp") == 0 || strcmp(argv[i], "--timestamp") == 0)
                opts.show_timestamp = 1;
            else if (strcmp(argv[i], "--json-view") == 0)
                opts.json_view = 1;
            else if (strcmp(argv[i], "--json-pretty") == 0)
                opts.json_pretty = 1;
            else if (strcmp(argv[i], "--remember-token") == 0)
                opts.remember_token = 1;
            else if (strcmp(argv[i], "--full-trace") == 0)
                opts.print_request = 1, opts.print_response = 1;
            else if (strcmp(argv[i], "--log-level") == 0 && i + 1 < argc)
                strncpy(opts.log_level, argv[++i], sizeof(opts.log_level) - 1);
            else if (strcmp(argv[i], "--color") == 0 && i + 1 < argc) {
                const char *mode = argv[++i];
                if (strcmp(mode, "always") == 0) opts.use_color = 1;
                else if (strcmp(mode, "never") == 0) opts.use_color = 0;
                else if (strcmp(mode, "auto") == 0) opts.use_color = bad_isatty_stdout();
                else {
                    fprintf(stderr, "[bad] invalid --color mode: %s\n", mode);
                    return 1;
                }
            }
            else if (strcmp(argv[i], "--no-color") == 0)
                opts.use_color = 0;
            else if (strcmp(argv[i], "--fail-fast") == 0)
                opts.fail_fast = 1;
            else if (strcmp(argv[i], "--strict-runtime-errors") == 0 || strcmp(argv[i], "--strict-runtime") == 0)
                opts.strict_runtime_errors = 1;
            else if (strcmp(argv[i], "--no-strict-runtime-errors") == 0)
                opts.strict_runtime_errors = 0;
            else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
                print_usage(); return 0;
            } else {
                fprintf(stderr, "[bad] unknown flag: %s\n", argv[i]);
                return 1;
            }
        } else {
            if (!filename) {
                filename = argv[i];
            } else {
                if (opts.arg_count >= 32) {
                    fprintf(stderr, "[bad] too many args (max 32)\n");
                    return 1;
                }
                strncpy(opts.args[opts.arg_count], argv[i], sizeof(opts.args[opts.arg_count]) - 1);
                opts.args[opts.arg_count][sizeof(opts.args[opts.arg_count]) - 1] = '\0';
                opts.arg_count++;
            }
        }
    }

    if (!filename) {
        fprintf(stderr, "[bad] no input file specified\n");
        print_usage();
        return 1;
    }

    if (opts.timeout_ms <= 0) opts.timeout_ms = 10000;
    normalize_spaces(opts.base_url);
    normalize_spaces(opts.history_dir);
    normalize_spaces(opts.history_file);
    normalize_spaces(opts.history_format);
    normalize_spaces(opts.history_mode);
    normalize_spaces(opts.history_methods);
    normalize_spaces(opts.history_exclude_methods);
    if (!opts.history_dir[0]) strncpy(opts.history_dir, ".bad-history", sizeof(opts.history_dir) - 1);
    if (!opts.history_mode[0]) strncpy(opts.history_mode, "all", sizeof(opts.history_mode) - 1);
    if (opts.history_max_body_bytes < 0) opts.history_max_body_bytes = 0;
    strncpy(opts.source_file, filename, sizeof(opts.source_file) - 1);

    /* Read and parse the .bad file */
    char *source = read_file(filename);

    if (opts.use_color) {
        printf("\n  " "\033[1m" "bad" "\033[0m"
               "  running: %s\n", filename);
    } else {
        printf("\n  bad  running: %s\n", filename);
    }
    if (opts.base_url[0])
        printf(opts.use_color ? "  " "\033[2m" "base: %s" "\033[0m\n" : "  base: %s\n", opts.base_url);

    lexer_init(source);
    ASTNode *root = parse();

    /* Execute */
    int failures = runtime_exec(root, &opts);

    ast_free(root);
    free(source);
    var_clear();

    return failures > 0 ? 1 : 0;
}
