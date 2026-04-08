#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bad.h"

static const char *src_buf;
static int         src_pos;
static int         src_line;
static Token       peeked;
static int         has_peek;

void lexer_init(const char *src) {
    src_buf  = src;
    src_pos  = 0;
    src_line = 1;
    has_peek = 0;
}

static char cur(void)  { return src_buf[src_pos]; }
static char peek1(void){ return src_buf[src_pos + 1]; }
static void adv(void)  {
    if (src_buf[src_pos] == '\n') src_line++;
    src_pos++;
}

static void skip_ws_comments(void) {
    for (;;) {
        while (isspace((unsigned char)cur())) adv();
        if (cur() == '/' && peek1() == '/') {
            while (cur() != '\n' && cur() != '\0') adv();
        } else if (cur() == '#') {
            while (cur() != '\n' && cur() != '\0') adv();
        } else {
            break;
        }
    }
}

static Token make_tok(BadTokenType t, const char *val) {
    Token tok;
    tok.type = t;
    tok.line = src_line;
    strncpy(tok.value, val, 511);
    tok.value[511] = '\0';
    return tok;
}

static Token read_string(char delim) {
    adv(); /* skip opening quote */
    char buf[512];
    int i = 0;
    while (cur() != delim && cur() != '\0') {
        if (cur() == '\\') {
            adv();
            switch (cur()) {
                case 'n':  buf[i++] = '\n'; break;
                case 't':  buf[i++] = '\t'; break;
                case '"':  buf[i++] = '"';  break;
                case '\'': buf[i++] = '\''; break;
                case '\\': buf[i++] = '\\'; break;
                default:   buf[i++] = cur();break;
            }
        } else {
            buf[i++] = cur();
        }
        adv();
        if (i >= 511) break;
    }
    buf[i] = '\0';
    if (cur() == delim) adv();
    return make_tok(TOKEN_STRING, buf);
}

static Token read_number(void) {
    char buf[64];
    int i = 0;
    int is_float = 0;
    while (isdigit((unsigned char)cur()) ||
           (cur() == '.' && !is_float && isdigit((unsigned char)peek1()))) {
        if (cur() == '.') is_float = 1;
        buf[i++] = cur();
        adv();
        if (i >= 63) break;
    }
    buf[i] = '\0';
    return make_tok(is_float ? TOKEN_FLOAT : TOKEN_INT, buf);
}

static Token read_ident(void) {
    char buf[256];
    int i = 0;
    while (isalnum((unsigned char)cur()) || cur() == '_' || cur() == '-') {
        buf[i++] = cur();
        adv();
        if (i >= 255) break;
    }
    buf[i] = '\0';

    /* keyword table */
    struct { const char *kw; BadTokenType t; } kws[] = {
        {"test",     TOKEN_TEST},
        {"send",     TOKEN_SEND},
        {"assert",   TOKEN_EXPECT},
        {"expect",   TOKEN_EXPECT},
        {"let",      TOKEN_LET},
        {"print",    TOKEN_PRINT},
        {"use",      TOKEN_IMPORT},
        {"import",   TOKEN_IMPORT},
        {"payload",  TOKEN_BODY},
        {"body",     TOKEN_BODY},
        {"headers",  TOKEN_HEADER},
        {"header",   TOKEN_HEADER},
        {"status",   TOKEN_STATUS},
        {"json",     TOKEN_JSON},
        {"exists",   TOKEN_EXISTS},
        {"base",     TOKEN_BASE_URL},
        {"base_url", TOKEN_BASE_URL},
        {"wait",     TOKEN_TIMEOUT},
        {"timeout",  TOKEN_TIMEOUT},
        {"only",     TOKEN_ONLY},
        {"export",   TOKEN_EXPORT},
        {"req",      TOKEN_REQUEST},
        {"request",  TOKEN_REQUEST},
        {"template", TOKEN_REQUEST},
        {"as",       TOKEN_AS},
        {"group",    TOKEN_GROUP},
        {"before_all", TOKEN_BEFORE_ALL},
        {"before_each", TOKEN_BEFORE_EACH},
        {"after_each", TOKEN_AFTER_EACH},
        {"after_all", TOKEN_AFTER_ALL},
        {"on_error", TOKEN_ON_ERROR},
        {"on_assertion_error", TOKEN_ON_ASSERTION_ERROR},
        {"on_network_error", TOKEN_ON_NETWORK_ERROR},
        {"before_url", TOKEN_BEFORE_URL},
        {"after_url", TOKEN_AFTER_URL},
        {"on_url_error", TOKEN_ON_URL_ERROR},
        {"skip",     TOKEN_SKIP},
        {"skip_if",  TOKEN_SKIP_IF},
        {"because",  TOKEN_BECAUSE},
        {"with",     TOKEN_WITH},
        {"if",       TOKEN_IF},
        {"and",      TOKEN_AND},
        {"or",       TOKEN_OR},
        {"not",      TOKEN_NOT},
        {"else",     TOKEN_ELSE},
        {"else_if",  TOKEN_ELSE_IF},
        {"retry",    TOKEN_RETRY},
        {"retry_delay_ms", TOKEN_RETRY_DELAY_MS},
        {"stop",     TOKEN_STOP},
        {"stop_all", TOKEN_STOP_ALL},
        {"sleep",    TOKEN_SLEEP},
        {"fail_if",  TOKEN_FAIL_IF},
        {"bearer",   TOKEN_BEARER},
        {"env",      TOKEN_ENV},
        {"args",     TOKEN_ARGS},
        {"time_start", TOKEN_TIME_START},
        {"time_stop",  TOKEN_TIME_STOP},
        {"time",       TOKEN_TIME},
        {"time_ms",    TOKEN_TIME_MS},
        {"now_ms",     TOKEN_NOW_MS},
        {"GET",      TOKEN_GET},
        {"POST",     TOKEN_POST},
        {"PUT",      TOKEN_PUT},
        {"PATCH",    TOKEN_PATCH},
        {"DELETE",   TOKEN_DELETE},
        {"true",     TOKEN_BOOL},
        {"false",    TOKEN_BOOL},
        {"null",     TOKEN_NULL_LIT},
        {NULL, 0}
    };
    for (int k = 0; kws[k].kw; k++) {
        if (strcmp(buf, kws[k].kw) == 0)
            return make_tok(kws[k].t, buf);
    }
    return make_tok(TOKEN_IDENT, buf);
}

static Token next_raw(void) {
    skip_ws_comments();
    char c = cur();
    if (c == '\0') return make_tok(TOKEN_EOF, "");
    if (c == '"' || c == '\'') return read_string(c);
    if (isdigit((unsigned char)c)) return read_number();
    if (isalpha((unsigned char)c) || c == '_') return read_ident();

    char buf[3] = {c, '\0', '\0'};
    adv();
    switch (c) {
        case '{': return make_tok(TOKEN_LBRACE, "{");
        case '}': return make_tok(TOKEN_RBRACE, "}");
        case '(': return make_tok(TOKEN_LPAREN, "(");
        case ')': return make_tok(TOKEN_RPAREN, ")");
        case '[': return make_tok(TOKEN_LBRACKET, "[");
        case ']': return make_tok(TOKEN_RBRACKET, "]");
        case ':': return make_tok(TOKEN_COLON,  ":");
        case '.': return make_tok(TOKEN_DOT,    ".");
        case ',': return make_tok(TOKEN_COMMA,  ",");
        case '=':
            if (cur() == '=') { adv(); return make_tok(TOKEN_EQ,  "=="); }
            return make_tok(TOKEN_ASSIGN, "=");
        case '!':
            if (cur() == '=') { adv(); return make_tok(TOKEN_NEQ, "!="); }
            break;
        case '<':
            if (cur() == '=') { adv(); return make_tok(TOKEN_LTE, "<="); }
            return make_tok(TOKEN_LT, "<");
        case '>':
            if (cur() == '=') { adv(); return make_tok(TOKEN_GTE, ">="); }
            return make_tok(TOKEN_GT, ">");
    }
    return make_tok(TOKEN_UNKNOWN, buf);
}

Token lexer_next(void) {
    if (has_peek) { has_peek = 0; return peeked; }
    return next_raw();
}

Token lexer_peek(void) {
    if (!has_peek) { peeked = next_raw(); has_peek = 1; }
    return peeked;
}
