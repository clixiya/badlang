#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bad.h"

/* ─── helpers ─────────────────────────────────────────────────── */
static ASTNode *new_node(ASTNodeType t) {
    ASTNode *n = calloc(1, sizeof(ASTNode));
    if (!n) { fprintf(stderr, "OOM\n"); exit(1); }
    n->type = t;
    return n;
}

static Token cur_tok;

static void advance(void) { cur_tok = lexer_next(); }

static void expect(BadTokenType t, const char *ctx) {
    if (cur_tok.type != t) {
        fprintf(stderr, "[bad] line %d: syntax error in %s — got '%s'\n",
                cur_tok.line, ctx, cur_tok.value);
        exit(1);
    }
    advance();
}

static int match(BadTokenType t) {
    if (cur_tok.type == t) { advance(); return 1; }
    return 0;
}

/* ─── forward decls ───────────────────────────────────────────── */
static ASTNode *parse_stmt(void);
static ASTNode *parse_value(void);
static ASTNode *parse_stats_ref(void);
static ASTNode *parse_object_literal(void);
static ASTNode *parse_body_source(void);
static ASTNode *parse_header_source(void);
static ASTNode *parse_print(void);
static ASTNode *parse_condition(void);
static ASTNode *parse_condition_or(void);
static ASTNode *parse_condition_and(void);
static ASTNode *parse_condition_not(void);
static ASTNode *parse_condition_primary(void);
static ASTNode *parse_list_literal(void);
static ASTNode *parse_if_stmt(void);
static ASTNode *parse_skip_if(void);
static ASTNode *parse_fail_if(void);
static ASTNode *parse_stop_stmt(ASTNodeType t, const char *ctx);
static ASTNode *parse_time_stmt(ASTNodeType t, const char *ctx);
static ASTNode *parse_sleep_stmt(void);
static ASTNode *parse_request_template(void);
static ASTNode *parse_export_decl(void);
static ASTNode *parse_hook_block(ASTNodeType t, const char *ctx, int require_pattern);

static ASTNode *parse_top_stmt(void);

static int token_can_be_key(BadTokenType t) {
    return t == TOKEN_STRING || t == TOKEN_IDENT ||
           t == TOKEN_BODY || t == TOKEN_HEADER || t == TOKEN_STATUS ||
           t == TOKEN_JSON || t == TOKEN_EXISTS ||
           t == TOKEN_PRINT || t == TOKEN_BEARER ||
           t == TOKEN_ENV || t == TOKEN_ARGS ||
           t == TOKEN_TIME || t == TOKEN_TIME_MS || t == TOKEN_NOW_MS ||
           t == TOKEN_TIME_START || t == TOKEN_TIME_STOP ||
           t == TOKEN_RETRY_DELAY_MS ||
           t == TOKEN_BASE_URL || t == TOKEN_TIMEOUT ||
           t == TOKEN_GET || t == TOKEN_POST || t == TOKEN_PUT ||
           t == TOKEN_PATCH || t == TOKEN_DELETE;
}

static ASTNode *parse_stats_ref(void) {
    ASTNode *n = new_node(AST_STATS_REF);
    char path[256] = {0};

    /* consume 'stats' identifier */
    advance();

    while (cur_tok.type == TOKEN_DOT) {
        advance();
        if (!(token_can_be_key(cur_tok.type) || cur_tok.type == TOKEN_INT)) {
            fprintf(stderr, "[bad] line %d: expected stats selector after '.'\n", cur_tok.line);
            exit(1);
        }
        if (path[0]) strncat(path, ".", sizeof(path) - strlen(path) - 1);
        strncat(path, cur_tok.value, sizeof(path) - strlen(path) - 1);
        advance();
    }

    strncpy(n->value, path, sizeof(n->value) - 1);
    n->value[sizeof(n->value) - 1] = '\0';
    return n;
}

/* ─── value: literals, ident refs, json paths, status refs, bearer wrappers ── */
static ASTNode *parse_value(void) {
    ASTNode *n;
    if (cur_tok.type == TOKEN_STRING) {
        n = new_node(AST_STRING);
        strcpy(n->value, cur_tok.value);
        advance(); return n;
    }
    if (cur_tok.type == TOKEN_INT) {
        n = new_node(AST_INT);
        n->int_val = atoi(cur_tok.value);
        strcpy(n->value, cur_tok.value);
        advance(); return n;
    }
    if (cur_tok.type == TOKEN_FLOAT) {
        n = new_node(AST_FLOAT);
        n->float_val = atof(cur_tok.value);
        strcpy(n->value, cur_tok.value);
        advance(); return n;
    }
    if (cur_tok.type == TOKEN_BOOL) {
        n = new_node(AST_BOOL);
        n->bool_val = strcmp(cur_tok.value, "true") == 0;
        strcpy(n->value, cur_tok.value);
        advance(); return n;
    }
    if (cur_tok.type == TOKEN_NULL_LIT) {
        n = new_node(AST_NULL);
        strcpy(n->value, "null");
        advance(); return n;
    }
    if (cur_tok.type == TOKEN_STATUS) {
        n = new_node(AST_STATUS_REF);
        strcpy(n->value, "status");
        advance(); return n;
    }
    if (cur_tok.type == TOKEN_JSON) {
        advance(); /* skip 'json' */

        char path[256] = {0};
        while (cur_tok.type == TOKEN_DOT) {
            advance();
            if (token_can_be_key(cur_tok.type) || cur_tok.type == TOKEN_INT) {
                if (path[0]) strncat(path, ".", sizeof(path) - strlen(path) - 1);
                strncat(path, cur_tok.value, sizeof(path) - strlen(path) - 1);
                advance();
            }
        }

        n = new_node(AST_JSON_PATH);
        strcpy(n->value, path);
        return n;
    }
    if (cur_tok.type == TOKEN_BEARER) {
        advance();
        n = new_node(AST_BEARER);
        n->left = parse_value();
        return n;
    }
    if (cur_tok.type == TOKEN_ENV) {
        advance();
        if (cur_tok.type != TOKEN_IDENT && cur_tok.type != TOKEN_STRING) {
            fprintf(stderr, "[bad] line %d: expected env variable name after 'env'\n", cur_tok.line);
            exit(1);
        }
        n = new_node(AST_ENV_REF);
        strncpy(n->value, cur_tok.value, sizeof(n->value) - 1);
        n->value[sizeof(n->value) - 1] = '\0';
        advance();
        return n;
    }
    if (cur_tok.type == TOKEN_ARGS) {
        advance();
        if (cur_tok.type == TOKEN_DOT) advance();
        if (cur_tok.type != TOKEN_INT) {
            fprintf(stderr, "[bad] line %d: expected integer index after 'args'\n", cur_tok.line);
            exit(1);
        }
        n = new_node(AST_ARG_REF);
        n->int_val = atoi(cur_tok.value);
        snprintf(n->value, sizeof(n->value), "%d", n->int_val);
        advance();
        return n;
    }
    if (cur_tok.type == TOKEN_TIME_MS) {
        n = new_node(AST_TIME_MS_REF);
        strcpy(n->value, "time_ms");
        advance();
        return n;
    }
    if (cur_tok.type == TOKEN_NOW_MS) {
        n = new_node(AST_NOW_MS_REF);
        strcpy(n->value, "now_ms");
        advance();
        return n;
    }
    if (cur_tok.type == TOKEN_LBRACE) {
        return parse_object_literal();
    }
    if (cur_tok.type == TOKEN_TIME) {
        advance();
        if (cur_tok.type != TOKEN_IDENT) {
            fprintf(stderr, "[bad] line %d: expected timer name after 'time'\n", cur_tok.line);
            exit(1);
        }
        n = new_node(AST_TIMER_REF);
        strcpy(n->value, cur_tok.value);
        advance();
        return n;
    }
    if (cur_tok.type == TOKEN_IDENT) {
        if (strcmp(cur_tok.value, "stats") == 0 && lexer_peek().type == TOKEN_DOT) {
            return parse_stats_ref();
        }
        n = new_node(AST_IDENT);
        strcpy(n->value, cur_tok.value);
        advance(); return n;
    }
    fprintf(stderr, "[bad] line %d: expected value, got '%s'\n",
            cur_tok.line, cur_tok.value);
    exit(1);
}

static ASTNode *make_spread_pair(ASTNodeType pair_type, ASTNode *val) {
    ASTNode *pair = new_node(pair_type);
    strcpy(pair->value, "__spread__");
    pair->left = val;
    return pair;
}

static ASTNode *parse_object_literal(void) {
    ASTNode *obj = new_node(AST_OBJECT);
    expect(TOKEN_LBRACE, "object literal");

    ASTNode *head = NULL;
    ASTNode **tail = &head;

    while (cur_tok.type != TOKEN_RBRACE && cur_tok.type != TOKEN_EOF) {
        if (cur_tok.type == TOKEN_IDENT && lexer_peek().type != TOKEN_COLON) {
            ASTNode *spread = make_spread_pair(AST_BODY_PAIR, parse_value());
            *tail = spread;
            tail = &spread->right;
            match(TOKEN_COMMA);
            continue;
        }

        char key[256] = {0};
        if (!token_can_be_key(cur_tok.type)) {
            fprintf(stderr, "[bad] line %d: expected key in object literal\n", cur_tok.line);
            exit(1);
        }
        strcpy(key, cur_tok.value);
        advance();
        expect(TOKEN_COLON, "object key:value");

        ASTNode *pair = new_node(AST_BODY_PAIR);
        strcpy(pair->value, key);
        pair->left = parse_value();
        *tail = pair;
        tail = &pair->right;
        match(TOKEN_COMMA);
    }

    expect(TOKEN_RBRACE, "object literal end");
    obj->left = head;
    return obj;
}

/* ─── body { key: value, ... } ───────────────────────────────── */
static ASTNode *parse_body_block(void) {
    expect(TOKEN_LBRACE, "body block");
    ASTNode *head = NULL, **tail = &head;
    while (cur_tok.type != TOKEN_RBRACE && cur_tok.type != TOKEN_EOF) {
        if (cur_tok.type == TOKEN_IDENT && lexer_peek().type != TOKEN_COLON) {
            ASTNode *spread = make_spread_pair(AST_BODY_PAIR, parse_value());
            *tail = spread;
            tail = &spread->right;
            match(TOKEN_COMMA);
            continue;
        }

        /* key — can be ident or string */
        char key[256] = {0};
        if (token_can_be_key(cur_tok.type)) {
            strcpy(key, cur_tok.value);
            advance();
        } else {
            fprintf(stderr, "[bad] line %d: expected key in body\n", cur_tok.line);
            exit(1);
        }
        expect(TOKEN_COLON, "body key:value");
        ASTNode *val = parse_value();

        ASTNode *pair = new_node(AST_BODY_PAIR);
        strcpy(pair->value, key);
        pair->left = val;
        *tail = pair;
        tail = &pair->right;

        match(TOKEN_COMMA); /* optional comma */
    }
    expect(TOKEN_RBRACE, "body block end");
    return head;
}

/* ─── header { Key: value, ... } ─────────────────────────────── */
static ASTNode *parse_header_block(void) {
    expect(TOKEN_LBRACE, "header block");
    ASTNode *head = NULL, **tail = &head;
    while (cur_tok.type != TOKEN_RBRACE && cur_tok.type != TOKEN_EOF) {
        if (cur_tok.type == TOKEN_IDENT && lexer_peek().type != TOKEN_COLON) {
            ASTNode *spread = make_spread_pair(AST_HEADER_PAIR, parse_value());
            *tail = spread;
            tail = &spread->right;
            match(TOKEN_COMMA);
            continue;
        }

        char key[256] = {0};
        if (token_can_be_key(cur_tok.type)) {
            strcpy(key, cur_tok.value);
            advance();
        } else {
            fprintf(stderr, "[bad] line %d: expected header key\n", cur_tok.line);
            exit(1);
        }
        expect(TOKEN_COLON, "header key:value");
        ASTNode *val = parse_value();

        ASTNode *pair = new_node(AST_HEADER_PAIR);
        strcpy(pair->value, key);
        pair->left = val;
        *tail = pair;
        tail = &pair->right;

        match(TOKEN_COMMA);
    }
    expect(TOKEN_RBRACE, "header block end");
    return head;
}

static ASTNode *parse_body_source(void) {
    if (cur_tok.type == TOKEN_LBRACE) {
        return parse_body_block();
    }

    ASTNode *val = parse_value();
    if (val->type != AST_IDENT && val->type != AST_OBJECT) {
        fprintf(stderr, "[bad] line %d: body source expects object variable or object literal\n", cur_tok.line);
        exit(1);
    }

    ASTNode *pair = make_spread_pair(AST_BODY_PAIR, val);
    return pair;
}

static ASTNode *parse_header_source(void) {
    if (cur_tok.type == TOKEN_LBRACE) {
        return parse_header_block();
    }

    ASTNode *val = parse_value();
    if (val->type != AST_IDENT && val->type != AST_OBJECT) {
        fprintf(stderr, "[bad] line %d: header source expects object variable or object literal\n", cur_tok.line);
        exit(1);
    }

    ASTNode *pair = make_spread_pair(AST_HEADER_PAIR, val);
    return pair;
}

/* ─── send METHOD "path" { optional body/header } ────────────── */
static ASTNode *parse_send(void) {
    ASTNode *n = new_node(AST_SEND);

    if (cur_tok.type == TOKEN_REQUEST) {
        advance();
        if (cur_tok.type != TOKEN_IDENT) {
            fprintf(stderr, "[bad] line %d: expected template name after 'send request'\n", cur_tok.line);
            exit(1);
        }
        strcpy(n->value, "__request_template__");
        ASTNode *tpl = new_node(AST_IDENT);
        strcpy(tpl->value, cur_tok.value);
        n->left = tpl;
        advance();

        if (match(TOKEN_WITH)) {
            expect(TOKEN_LBRACE, "request override block");
            while (cur_tok.type != TOKEN_RBRACE && cur_tok.type != TOKEN_EOF) {
                if (cur_tok.type == TOKEN_IDENT && strcmp(cur_tok.value, "path") == 0) {
                    advance();
                    n->extra = parse_value();
                } else if (cur_tok.type == TOKEN_BODY) {
                    advance();
                    n->body = parse_body_source();
                } else if (cur_tok.type == TOKEN_HEADER) {
                    advance();
                    n->headers = parse_header_source();
                } else if (cur_tok.type == TOKEN_IDENT &&
                           (strcmp(cur_tok.value, "body_merge") == 0 || strcmp(cur_tok.value, "merge_body") == 0)) {
                    advance();
                    n->alt = parse_value();
                } else if (cur_tok.type == TOKEN_RETRY) {
                    advance();
                    n->retry = parse_value();
                } else if (cur_tok.type == TOKEN_RETRY_DELAY_MS ||
                           (cur_tok.type == TOKEN_IDENT && strcmp(cur_tok.value, "retry_delay_ms") == 0)) {
                    advance();
                    n->retry_delay = parse_value();
                } else if (cur_tok.type == TOKEN_IDENT && strcmp(cur_tok.value, "retry_backoff") == 0) {
                    advance();
                    n->retry_backoff = parse_value();
                } else if (cur_tok.type == TOKEN_IDENT && strcmp(cur_tok.value, "retry_jitter_ms") == 0) {
                    advance();
                    n->retry_jitter = parse_value();
                } else {
                    fprintf(stderr, "[bad] line %d: expected path/body/header/body_merge/retry/retry_delay_ms/retry_backoff/retry_jitter_ms in request override block\n", cur_tok.line);
                    exit(1);
                }
            }
            expect(TOKEN_RBRACE, "request override block end");
        }

        return n;
    }

    /* HTTP method */
    if (cur_tok.type == TOKEN_GET  || cur_tok.type == TOKEN_POST  ||
        cur_tok.type == TOKEN_PUT  || cur_tok.type == TOKEN_PATCH ||
        cur_tok.type == TOKEN_DELETE) {
        strcpy(n->value, cur_tok.value);   /* store method */
        advance();
    } else {
        fprintf(stderr, "[bad] line %d: expected HTTP method\n", cur_tok.line);
        exit(1);
    }

    /* path — string literal or ident (variable) */
    if (cur_tok.type == TOKEN_STRING) {
        strcpy(n->op, "literal");
        /* reuse left->value for path */
        ASTNode *path = new_node(AST_STRING);
        strcpy(path->value, cur_tok.value);
        n->left = path;
        advance();
    } else if (cur_tok.type == TOKEN_IDENT) {
        ASTNode *path = new_node(AST_IDENT);
        strcpy(path->value, cur_tok.value);
        n->left = path;
        advance();
    } else {
        fprintf(stderr, "[bad] line %d: expected URL path after method\n", cur_tok.line);
        exit(1);
    }

    /* optional { body { } header { } } block */
    if (cur_tok.type == TOKEN_LBRACE) {
        advance(); /* enter outer block */
        while (cur_tok.type != TOKEN_RBRACE && cur_tok.type != TOKEN_EOF) {
            if (cur_tok.type == TOKEN_BODY) {
                advance();
                n->body = parse_body_source();
            } else if (cur_tok.type == TOKEN_HEADER) {
                advance();
                n->headers = parse_header_source();
            } else if (cur_tok.type == TOKEN_RETRY) {
                advance();
                n->retry = parse_value();
            } else if (cur_tok.type == TOKEN_RETRY_DELAY_MS ||
                       (cur_tok.type == TOKEN_IDENT && strcmp(cur_tok.value, "retry_delay_ms") == 0)) {
                advance();
                n->retry_delay = parse_value();
            } else if (cur_tok.type == TOKEN_IDENT && strcmp(cur_tok.value, "retry_backoff") == 0) {
                advance();
                n->retry_backoff = parse_value();
            } else if (cur_tok.type == TOKEN_IDENT && strcmp(cur_tok.value, "retry_jitter_ms") == 0) {
                advance();
                n->retry_jitter = parse_value();
            } else {
                fprintf(stderr, "[bad] line %d: expected 'body', 'header', 'retry', 'retry_delay_ms', 'retry_backoff', or 'retry_jitter_ms', got '%s'\n",
                        cur_tok.line, cur_tok.value);
                exit(1);
            }
        }
        expect(TOKEN_RBRACE, "send block end");
    }
    return n;
}

/* ─── expect … ────────────────────────────────────────────────── */
static int is_comparator_token(BadTokenType t) {
    return t == TOKEN_EQ || t == TOKEN_NEQ ||
           t == TOKEN_LT || t == TOKEN_GT ||
           t == TOKEN_LTE || t == TOKEN_GTE;
}

static int is_word_operator(const char *s) {
    return s && (
        strcmp(s, "contains") == 0 ||
        strcmp(s, "starts_with") == 0 ||
        strcmp(s, "ends_with") == 0 ||
        strcmp(s, "regex") == 0 ||
        strcmp(s, "in") == 0
    );
}

static ASTNode *parse_list_literal(void) {
    ASTNode *list = new_node(AST_LIST);
    expect(TOKEN_LBRACKET, "list literal");

    ASTNode *head = NULL;
    ASTNode **tail = &head;

    while (cur_tok.type != TOKEN_RBRACKET && cur_tok.type != TOKEN_EOF) {
        ASTNode *item = parse_value();
        *tail = item;
        tail = &item->right;

        if (!match(TOKEN_COMMA)) break;
    }

    expect(TOKEN_RBRACKET, "list literal end");
    list->left = head;
    return list;
}

static ASTNode *parse_expect(void) {
    /* expect status ... */
    if (cur_tok.type == TOKEN_STATUS) {
        advance();
        ASTNode *n = new_node(AST_EXPECT_STATUS);

        if (cur_tok.type == TOKEN_INT) {
            n->int_val = atoi(cur_tok.value);
            advance();
            return n;
        }

        n->left = new_node(AST_STATUS_REF);
        strcpy(n->left->value, "status");

        if (is_comparator_token(cur_tok.type)) {
            strcpy(n->op, cur_tok.value);
            advance();
            n->extra = parse_value();
            return n;
        }

        if (cur_tok.type == TOKEN_IDENT && strcmp(cur_tok.value, "in") == 0) {
            strcpy(n->op, "in");
            advance();
            n->extra = parse_list_literal();
            return n;
        }

        fprintf(stderr, "[bad] line %d: expected status code or comparison after 'expect status'\n", cur_tok.line);
        exit(1);
    }

    /* expect time_ms/time/now_ms <op> value | in [..] */
    if (cur_tok.type == TOKEN_TIME_MS || cur_tok.type == TOKEN_TIME || cur_tok.type == TOKEN_NOW_MS) {
        ASTNode *n = new_node(AST_EXPECT_TIME);
        n->left = parse_value();

        if (is_comparator_token(cur_tok.type)) {
            strcpy(n->op, cur_tok.value);
            advance();
            n->extra = parse_value();
            return n;
        }

        if (cur_tok.type == TOKEN_IDENT && strcmp(cur_tok.value, "in") == 0) {
            strcpy(n->op, "in");
            advance();
            n->extra = parse_list_literal();
            return n;
        }

        fprintf(stderr, "[bad] line %d: expected comparison or 'in [..]' after time expression in expect\n", cur_tok.line);
        exit(1);
    }

    /* expect json.some.path [exists | op value | in [..]] */
    if (cur_tok.type == TOKEN_JSON) {
        advance(); /* skip 'json' */

        char path[256] = {0};
        while (cur_tok.type == TOKEN_DOT) {
            advance();
            if (token_can_be_key(cur_tok.type) || cur_tok.type == TOKEN_INT) {
                if (path[0]) strncat(path, ".", sizeof(path) - strlen(path) - 1);
                strncat(path, cur_tok.value, sizeof(path) - strlen(path) - 1);
                advance();
            }
        }

        ASTNode *n = new_node(AST_EXPECT_JSON);
        strcpy(n->value, path);

        if (cur_tok.type == TOKEN_EXISTS) {
            strcpy(n->op, "exists");
            advance();
            return n;
        }

        if (is_comparator_token(cur_tok.type)) {
            strcpy(n->op, cur_tok.value);
            advance();
            n->extra = parse_value();
            return n;
        }

        if (cur_tok.type == TOKEN_IDENT && is_word_operator(cur_tok.value)) {
            strcpy(n->op, cur_tok.value);
            advance();

            if (strcmp(n->op, "in") == 0) {
                n->extra = parse_list_literal();
            } else {
                n->extra = parse_value();
            }
            return n;
        }

        strcpy(n->op, "exists");
        return n;
    }

    fprintf(stderr, "[bad] line %d: expected 'status', 'json', or time expression after expect\n",
            cur_tok.line);
    exit(1);
}

/* ─── let name = send … | values/json/status/bearer ───────────── */
static ASTNode *parse_let(void) {
    ASTNode *n = new_node(AST_LET);
    if (cur_tok.type != TOKEN_IDENT) {
        fprintf(stderr, "[bad] line %d: expected variable name after let\n", cur_tok.line);
        exit(1);
    }
    strcpy(n->value, cur_tok.value); /* var name */
    advance();
    expect(TOKEN_ASSIGN, "let assignment");

    if (cur_tok.type == TOKEN_SEND) {
        advance();
        n->left = parse_send(); /* captures full send node */
    } else {
        n->left = parse_value();
    }
    return n;
}

/* ─── print value ─────────────────────────────────────────────── */
static ASTNode *parse_print(void) {
    ASTNode *n = new_node(AST_PRINT);
    n->left = parse_value();
    return n;
}

/* ─── condition primary: <value> [exists|op <value>] ─────────── */
static ASTNode *parse_condition_primary(void) {
    if (cur_tok.type == TOKEN_AND || cur_tok.type == TOKEN_OR) {
        fprintf(stderr, "[bad] line %d: malformed condition — unexpected '%s' at condition start\n", cur_tok.line, cur_tok.value);
        exit(1);
    }

    if (cur_tok.type == TOKEN_LBRACE || cur_tok.type == TOKEN_RBRACE ||
        cur_tok.type == TOKEN_RPAREN || cur_tok.type == TOKEN_EOF) {
        fprintf(stderr, "[bad] line %d: malformed condition — expected expression\n", cur_tok.line);
        exit(1);
    }

    ASTNode *n = new_node(AST_COMPARE);
    n->left = parse_value();

    if (cur_tok.type == TOKEN_EXISTS) {
        strcpy(n->op, "exists");
        advance();
        return n;
    }

    BadTokenType op_t = cur_tok.type;
    if (op_t == TOKEN_EQ || op_t == TOKEN_NEQ ||
        op_t == TOKEN_LT || op_t == TOKEN_GT  ||
        op_t == TOKEN_LTE|| op_t == TOKEN_GTE) {
        strcpy(n->op, cur_tok.value);
        advance();
        n->extra = parse_value();
        return n;
    }

    if (cur_tok.type == TOKEN_IDENT && is_word_operator(cur_tok.value)) {
        strcpy(n->op, cur_tok.value);
        advance();

        if (strcmp(n->op, "in") == 0) {
            n->extra = parse_list_literal();
        } else {
            n->extra = parse_value();
        }
        return n;
    }

    strcpy(n->op, "truthy");
    return n;
}

/* ─── condition unary: [not] <condition> ─────────────────────── */
static ASTNode *parse_condition_not(void) {
    if (match(TOKEN_NOT)) {
        if (cur_tok.type == TOKEN_AND || cur_tok.type == TOKEN_OR ||
            cur_tok.type == TOKEN_LBRACE ||
            cur_tok.type == TOKEN_RBRACE || cur_tok.type == TOKEN_RPAREN ||
            cur_tok.type == TOKEN_EOF) {
            fprintf(stderr, "[bad] line %d: malformed condition — expected expression after 'not'\n", cur_tok.line);
            exit(1);
        }

        ASTNode *n = new_node(AST_COMPARE);
        strcpy(n->op, "not");
        n->left = parse_condition_not();
        return n;
    }

    if (match(TOKEN_LPAREN)) {
        ASTNode *inner = parse_condition();
        expect(TOKEN_RPAREN, "grouped condition") ;
        return inner;
    }

    return parse_condition_primary();
}

/* ─── condition and-chain: a and b and c ─────────────────────── */
static ASTNode *parse_condition_and(void) {
    ASTNode *left = parse_condition_not();
    while (match(TOKEN_AND)) {
        if (cur_tok.type == TOKEN_AND || cur_tok.type == TOKEN_OR ||
            cur_tok.type == TOKEN_LBRACE ||
            cur_tok.type == TOKEN_RBRACE || cur_tok.type == TOKEN_RPAREN ||
            cur_tok.type == TOKEN_EOF) {
            fprintf(stderr, "[bad] line %d: malformed condition — missing right-hand expression after 'and'\n", cur_tok.line);
            exit(1);
        }

        ASTNode *n = new_node(AST_COMPARE);
        strcpy(n->op, "and");
        n->left = left;
        n->extra = parse_condition_not();
        left = n;
    }
    return left;
}

/* ─── condition or-chain: a or b or c ────────────────────────── */
static ASTNode *parse_condition_or(void) {
    ASTNode *left = parse_condition_and();
    while (match(TOKEN_OR)) {
        if (cur_tok.type == TOKEN_AND || cur_tok.type == TOKEN_OR ||
            cur_tok.type == TOKEN_LBRACE ||
            cur_tok.type == TOKEN_RBRACE || cur_tok.type == TOKEN_RPAREN ||
            cur_tok.type == TOKEN_EOF) {
            fprintf(stderr, "[bad] line %d: malformed condition — missing right-hand expression after 'or'\n", cur_tok.line);
            exit(1);
        }

        ASTNode *n = new_node(AST_COMPARE);
        strcpy(n->op, "or");
        n->left = left;
        n->extra = parse_condition_and();
        left = n;
    }
    return left;
}

/* ─── condition: supports not/and/or with precedence ─────────── */
static ASTNode *parse_condition(void) {
    return parse_condition_or();
}

/* ─── if <cond> { ... } [else { ... }] ───────────────────────── */
static ASTNode *parse_if_stmt(void) {
    ASTNode *n = new_node(AST_IF);
    n->left = parse_condition();

    expect(TOKEN_LBRACE, "if block");
    ASTNode *head = NULL, **tail = &head;
    while (cur_tok.type != TOKEN_RBRACE && cur_tok.type != TOKEN_EOF) {
        ASTNode *stmt = parse_stmt();
        *tail = stmt;
        tail = &stmt->right;
    }
    expect(TOKEN_RBRACE, "if block end");
    n->stmts = head;

    if (match(TOKEN_ELSE_IF)) {
        n->alt = parse_if_stmt();
        return n;
    }

    if (match(TOKEN_ELSE)) {
        if (match(TOKEN_IF)) {
            n->alt = parse_if_stmt();
            return n;
        }

        expect(TOKEN_LBRACE, "else block");
        ASTNode *ehead = NULL, **etail = &ehead;
        while (cur_tok.type != TOKEN_RBRACE && cur_tok.type != TOKEN_EOF) {
            ASTNode *stmt = parse_stmt();
            *etail = stmt;
            etail = &stmt->right;
        }
        expect(TOKEN_RBRACE, "else block end");
        n->alt = ehead;
    }

    return n;
}

/* ─── skip_if <cond> [because "..."] ────────────────────────── */
static ASTNode *parse_skip_if(void) {
    ASTNode *n = new_node(AST_SKIP_IF);
    n->left = parse_condition();

    if (match(TOKEN_BECAUSE)) {
        if (cur_tok.type != TOKEN_STRING) {
            fprintf(stderr, "[bad] line %d: expected reason string after 'because'\n", cur_tok.line);
            exit(1);
        }
        strncpy(n->skip_reason, cur_tok.value, sizeof(n->skip_reason) - 1);
        n->skip_reason[sizeof(n->skip_reason) - 1] = '\0';
        advance();
    }
    return n;
}

/* ─── fail_if <cond> [because "..."] ───────────────────────── */
static ASTNode *parse_fail_if(void) {
    ASTNode *n = new_node(AST_FAIL_IF);
    n->left = parse_condition();

    if (match(TOKEN_BECAUSE)) {
        if (cur_tok.type != TOKEN_STRING) {
            fprintf(stderr, "[bad] line %d: expected reason string after 'because'\n", cur_tok.line);
            exit(1);
        }
        strncpy(n->skip_reason, cur_tok.value, sizeof(n->skip_reason) - 1);
        n->skip_reason[sizeof(n->skip_reason) - 1] = '\0';
        advance();
    }
    return n;
}

/* ─── stop / stop_all [because "..."] ───────────────────────── */
static ASTNode *parse_stop_stmt(ASTNodeType t, const char *ctx) {
    ASTNode *n = new_node(t);
    if (match(TOKEN_BECAUSE)) {
        if (cur_tok.type != TOKEN_STRING) {
            fprintf(stderr, "[bad] line %d: expected reason string after 'because' in %s\n", cur_tok.line, ctx);
            exit(1);
        }
        strncpy(n->skip_reason, cur_tok.value, sizeof(n->skip_reason) - 1);
        n->skip_reason[sizeof(n->skip_reason) - 1] = '\0';
        advance();
    }
    return n;
}

/* ─── time_start/time_stop <timer_name> ─────────────────────── */
static ASTNode *parse_time_stmt(ASTNodeType t, const char *ctx) {
    ASTNode *n = new_node(t);
    if (cur_tok.type != TOKEN_IDENT) {
        fprintf(stderr, "[bad] line %d: expected timer name after %s\n", cur_tok.line, ctx);
        exit(1);
    }
    strncpy(n->value, cur_tok.value, sizeof(n->value) - 1);
    n->value[sizeof(n->value) - 1] = '\0';
    advance();
    return n;
}

/* ─── sleep <ms> ─────────────────────────────────────────────── */
static ASTNode *parse_sleep_stmt(void) {
    ASTNode *n = new_node(AST_SLEEP);
    n->left = parse_value();
    return n;
}

/* ─── statements inside a test block ─────────────────────────── */
static ASTNode *parse_stmt(void) {
    if (cur_tok.type == TOKEN_SEND) {
        advance();
        return parse_send();
    }
    if (cur_tok.type == TOKEN_EXPECT) {
        advance();
        return parse_expect();
    }
    if (cur_tok.type == TOKEN_LET) {
        advance();
        return parse_let();
    }
    if (cur_tok.type == TOKEN_PRINT) {
        advance();
        return parse_print();
    }
    if (cur_tok.type == TOKEN_IF) {
        advance();
        return parse_if_stmt();
    }
    if (cur_tok.type == TOKEN_SKIP_IF) {
        advance();
        return parse_skip_if();
    }
    if (cur_tok.type == TOKEN_FAIL_IF) {
        advance();
        return parse_fail_if();
    }
    if (cur_tok.type == TOKEN_STOP) {
        advance();
        return parse_stop_stmt(AST_STOP, "stop");
    }
    if (cur_tok.type == TOKEN_STOP_ALL) {
        advance();
        return parse_stop_stmt(AST_STOP_ALL, "stop_all");
    }
    if (cur_tok.type == TOKEN_TIME_START) {
        advance();
        return parse_time_stmt(AST_TIME_START, "time_start");
    }
    if (cur_tok.type == TOKEN_TIME_STOP) {
        advance();
        return parse_time_stmt(AST_TIME_STOP, "time_stop");
    }
    if (cur_tok.type == TOKEN_SLEEP) {
        advance();
        return parse_sleep_stmt();
    }
    fprintf(stderr, "[bad] line %d: unexpected token '%s' in test body\n",
            cur_tok.line, cur_tok.value);
    exit(1);
}

static ASTNode *parse_before_all(void) {
    ASTNode *n = new_node(AST_BEFORE_ALL);
    expect(TOKEN_LBRACE, "before_all block");

    ASTNode *head = NULL, **tail = &head;
    while (cur_tok.type != TOKEN_RBRACE && cur_tok.type != TOKEN_EOF) {
        ASTNode *stmt = parse_stmt();
        *tail = stmt;
        tail = &stmt->right;
    }
    expect(TOKEN_RBRACE, "before_all block end");
    n->stmts = head;
    return n;
}

/* ─── test "name" { stmts } ─────────────────────────────────── */
static ASTNode *parse_test(void) {
    ASTNode *n = new_node(AST_TEST);

    if (cur_tok.type != TOKEN_STRING) {
        fprintf(stderr, "[bad] line %d: expected test name (string)\n", cur_tok.line);
        exit(1);
    }
    strcpy(n->value, cur_tok.value);
    advance();

    if (match(TOKEN_BECAUSE)) {
        if (cur_tok.type != TOKEN_STRING) {
            fprintf(stderr, "[bad] line %d: expected reason string after 'because'\n", cur_tok.line);
            exit(1);
        }
        strncpy(n->skip_reason, cur_tok.value, sizeof(n->skip_reason) - 1);
        n->skip_reason[sizeof(n->skip_reason) - 1] = '\0';
        advance();
    }

    expect(TOKEN_LBRACE, "test block");

    ASTNode *head = NULL, **tail = &head;
    while (cur_tok.type != TOKEN_RBRACE && cur_tok.type != TOKEN_EOF) {
        ASTNode *stmt = parse_stmt();
        *tail = stmt;
        tail = &stmt->right; /* chain via right */
    }
    expect(TOKEN_RBRACE, "test block end");
    n->stmts = head;
    return n;
}

/* ─── import "file.bad" ──────────────────────────────────────── */
static ASTNode *parse_import(void) {
    ASTNode *n = new_node(AST_IMPORT);
    if (cur_tok.type != TOKEN_STRING) {
        fprintf(stderr, "[bad] line %d: expected filename after import\n", cur_tok.line);
        exit(1);
    }
    strcpy(n->value, cur_tok.value);
    advance();

    if (cur_tok.type == TOKEN_ONLY) {
        Token pk = lexer_peek();
        if (pk.type != TOKEN_IDENT) {
            return n;
        }
        advance();
        char list[512] = {0};
        int first = 1;
        while (cur_tok.type == TOKEN_IDENT) {
            char src[128] = {0};
            char alias[128] = {0};

            strncpy(src, cur_tok.value, sizeof(src) - 1);
            strncpy(alias, cur_tok.value, sizeof(alias) - 1);
            advance();

            if (match(TOKEN_AS)) {
                if (cur_tok.type != TOKEN_IDENT) {
                    fprintf(stderr, "[bad] line %d: expected alias name after 'as'\n", cur_tok.line);
                    exit(1);
                }
                strncpy(alias, cur_tok.value, sizeof(alias) - 1);
                advance();
            }

            if (!first) strncat(list, ",", sizeof(list) - strlen(list) - 1);
            strncat(list, src, sizeof(list) - strlen(list) - 1);
            strncat(list, "=", sizeof(list) - strlen(list) - 1);
            strncat(list, alias, sizeof(list) - strlen(list) - 1);
            first = 0;
            if (!match(TOKEN_COMMA)) break;
        }

        if (first) {
            fprintf(stderr, "[bad] line %d: expected symbol name after 'only'\n", cur_tok.line);
            exit(1);
        }

        ASTNode *sel = new_node(AST_STRING);
        strcpy(sel->value, list);
        n->extra = sel;
    }
    return n;
}

static ASTNode *parse_request_template(void) {
    ASTNode *n = new_node(AST_REQUEST_TEMPLATE);
    if (cur_tok.type != TOKEN_IDENT) {
        fprintf(stderr, "[bad] line %d: expected template name after request\n", cur_tok.line);
        exit(1);
    }
    strcpy(n->value, cur_tok.value);
    advance();

    ASTNode *expect_head = NULL;
    ASTNode **expect_tail = &expect_head;

    expect(TOKEN_LBRACE, "request block");
    while (cur_tok.type != TOKEN_RBRACE && cur_tok.type != TOKEN_EOF) {
        if (cur_tok.type == TOKEN_IDENT && strcmp(cur_tok.value, "method") == 0) {
            advance();
            if (cur_tok.type == TOKEN_GET || cur_tok.type == TOKEN_POST || cur_tok.type == TOKEN_PUT ||
                cur_tok.type == TOKEN_PATCH || cur_tok.type == TOKEN_DELETE) {
                strncpy(n->op, cur_tok.value, sizeof(n->op) - 1);
                advance();
            } else {
                fprintf(stderr, "[bad] line %d: expected HTTP method in request template\n", cur_tok.line);
                exit(1);
            }
        } else if (cur_tok.type == TOKEN_IDENT && strcmp(cur_tok.value, "path") == 0) {
            advance();
            n->left = parse_value();
        } else if (cur_tok.type == TOKEN_BODY) {
            advance();
            n->body = parse_body_source();
        } else if (cur_tok.type == TOKEN_HEADER) {
            advance();
            n->headers = parse_header_source();
        } else if (cur_tok.type == TOKEN_RETRY) {
            advance();
            n->retry = parse_value();
        } else if (cur_tok.type == TOKEN_RETRY_DELAY_MS ||
                   (cur_tok.type == TOKEN_IDENT && strcmp(cur_tok.value, "retry_delay_ms") == 0)) {
            advance();
            n->retry_delay = parse_value();
        } else if (cur_tok.type == TOKEN_IDENT && strcmp(cur_tok.value, "retry_backoff") == 0) {
            advance();
            n->retry_backoff = parse_value();
        } else if (cur_tok.type == TOKEN_IDENT && strcmp(cur_tok.value, "retry_jitter_ms") == 0) {
            advance();
            n->retry_jitter = parse_value();
        } else if (cur_tok.type == TOKEN_EXPECT) {
            advance();
            ASTNode *e = parse_expect();
            *expect_tail = e;
            expect_tail = &e->right;
        } else {
            fprintf(stderr, "[bad] line %d: expected method/path/body/header/retry/retry_delay_ms/retry_backoff/retry_jitter_ms/expect in request template\n", cur_tok.line);
            exit(1);
        }
    }
    expect(TOKEN_RBRACE, "request block end");
    n->stmts = expect_head;

    if (!n->op[0]) {
        fprintf(stderr, "[bad] request template '%s' missing method\n", n->value);
        exit(1);
    }
    return n;
}

static ASTNode *parse_export_decl(void) {
    if (cur_tok.type == TOKEN_LET) {
        advance();
        ASTNode *n = parse_let();
        n->is_export = 1;
        return n;
    }
    if (cur_tok.type == TOKEN_REQUEST) {
        advance();
        ASTNode *n = parse_request_template();
        n->is_export = 1;
        return n;
    }
    fprintf(stderr, "[bad] line %d: export supports only let/request declarations\n", cur_tok.line);
    exit(1);
}

static ASTNode *parse_before_each(void) {
    ASTNode *n = new_node(AST_BEFORE_EACH);
    expect(TOKEN_LBRACE, "before_each block");

    ASTNode *head = NULL, **tail = &head;
    while (cur_tok.type != TOKEN_RBRACE && cur_tok.type != TOKEN_EOF) {
        ASTNode *stmt = parse_stmt();
        *tail = stmt;
        tail = &stmt->right;
    }
    expect(TOKEN_RBRACE, "before_each block end");
    n->stmts = head;
    return n;
}

static ASTNode *parse_after_each(void) {
    ASTNode *n = new_node(AST_AFTER_EACH);
    expect(TOKEN_LBRACE, "after_each block");

    ASTNode *head = NULL, **tail = &head;
    while (cur_tok.type != TOKEN_RBRACE && cur_tok.type != TOKEN_EOF) {
        ASTNode *stmt = parse_stmt();
        *tail = stmt;
        tail = &stmt->right;
    }
    expect(TOKEN_RBRACE, "after_each block end");
    n->stmts = head;
    return n;
}

static ASTNode *parse_after_all(void) {
    ASTNode *n = new_node(AST_AFTER_ALL);
    expect(TOKEN_LBRACE, "after_all block");

    ASTNode *head = NULL, **tail = &head;
    while (cur_tok.type != TOKEN_RBRACE && cur_tok.type != TOKEN_EOF) {
        ASTNode *stmt = parse_stmt();
        *tail = stmt;
        tail = &stmt->right;
    }
    expect(TOKEN_RBRACE, "after_all block end");
    n->stmts = head;
    return n;
}

static ASTNode *parse_hook_block(ASTNodeType t, const char *ctx, int require_pattern) {
    ASTNode *n = new_node(t);

    if (require_pattern) {
        if (cur_tok.type != TOKEN_STRING) {
            fprintf(stderr, "[bad] line %d: expected URL pattern string after %s\n", cur_tok.line, ctx);
            exit(1);
        }
        strncpy(n->value, cur_tok.value, sizeof(n->value) - 1);
        n->value[sizeof(n->value) - 1] = '\0';
        advance();
    }

    expect(TOKEN_LBRACE, ctx);
    ASTNode *head = NULL, **tail = &head;
    while (cur_tok.type != TOKEN_RBRACE && cur_tok.type != TOKEN_EOF) {
        ASTNode *stmt = parse_stmt();
        *tail = stmt;
        tail = &stmt->right;
    }
    expect(TOKEN_RBRACE, "hook block end");
    n->stmts = head;
    return n;
}

static ASTNode *parse_group(void) {
    ASTNode *n = new_node(AST_GROUP);
    if (cur_tok.type != TOKEN_STRING) {
        fprintf(stderr, "[bad] line %d: expected group name string\n", cur_tok.line);
        exit(1);
    }
    strcpy(n->value, cur_tok.value);
    advance();

    if (match(TOKEN_BECAUSE)) {
        if (cur_tok.type != TOKEN_STRING) {
            fprintf(stderr, "[bad] line %d: expected reason string after 'because'\n", cur_tok.line);
            exit(1);
        }
        strncpy(n->skip_reason, cur_tok.value, sizeof(n->skip_reason) - 1);
        n->skip_reason[sizeof(n->skip_reason) - 1] = '\0';
        advance();
    }

    expect(TOKEN_LBRACE, "group block");
    ASTNode *head = NULL, **tail = &head;
    while (cur_tok.type != TOKEN_RBRACE && cur_tok.type != TOKEN_EOF) {
        ASTNode *stmt = parse_top_stmt();
        *tail = stmt;
        tail = &stmt->right;
    }
    expect(TOKEN_RBRACE, "group block end");
    n->stmts = head;
    return n;
}

static ASTNode *parse_top_stmt(void) {
    ASTNode *stmt = NULL;

    if (cur_tok.type == TOKEN_TEST) {
        advance();
        stmt = parse_test();
    } else if (cur_tok.type == TOKEN_SKIP) {
        advance();
        if (cur_tok.type == TOKEN_TEST) {
            advance();
            if (cur_tok.type != TOKEN_STRING) {
                fprintf(stderr, "[bad] line %d: expected test name (string) after 'skip test'\n", cur_tok.line);
                exit(1);
            }
            ASTNode *t = new_node(AST_TEST);
            strncpy(t->value, cur_tok.value, sizeof(t->value) - 1);
            advance();

            if (match(TOKEN_BECAUSE)) {
                if (cur_tok.type != TOKEN_STRING) {
                    fprintf(stderr, "[bad] line %d: expected reason string after 'because'\n", cur_tok.line);
                    exit(1);
                }
                strncpy(t->skip_reason, cur_tok.value, sizeof(t->skip_reason) - 1);
                advance();
            }

            if (cur_tok.type == TOKEN_LBRACE) {
                advance();
                ASTNode *head = NULL, **tail = &head;
                while (cur_tok.type != TOKEN_RBRACE && cur_tok.type != TOKEN_EOF) {
                    ASTNode *s = parse_stmt();
                    *tail = s;
                    tail = &s->right;
                }
                expect(TOKEN_RBRACE, "skip test block end");
                t->stmts = head;
            }

            t->is_skip = 1;
            stmt = t;
        } else if (cur_tok.type == TOKEN_GROUP) {
            advance();
            if (cur_tok.type != TOKEN_STRING) {
                fprintf(stderr, "[bad] line %d: expected group name string after 'skip group'\n", cur_tok.line);
                exit(1);
            }
            ASTNode *g = new_node(AST_GROUP);
            strncpy(g->value, cur_tok.value, sizeof(g->value) - 1);
            advance();

            if (match(TOKEN_BECAUSE)) {
                if (cur_tok.type != TOKEN_STRING) {
                    fprintf(stderr, "[bad] line %d: expected reason string after 'because'\n", cur_tok.line);
                    exit(1);
                }
                strncpy(g->skip_reason, cur_tok.value, sizeof(g->skip_reason) - 1);
                advance();
            }

            if (cur_tok.type == TOKEN_LBRACE) {
                advance();
                ASTNode *head = NULL, **tail = &head;
                while (cur_tok.type != TOKEN_RBRACE && cur_tok.type != TOKEN_EOF) {
                    ASTNode *s = parse_top_stmt();
                    *tail = s;
                    tail = &s->right;
                }
                expect(TOKEN_RBRACE, "skip group block end");
                g->stmts = head;
            }

            g->is_skip = 1;
            stmt = g;
        } else {
            fprintf(stderr, "[bad] line %d: expected 'test' or 'group' after 'skip'\n", cur_tok.line);
            exit(1);
        }
    } else if (cur_tok.type == TOKEN_ONLY) {
        advance();
        if (cur_tok.type == TOKEN_TEST) {
            advance();
            stmt = parse_test();
            stmt->is_only = 1;
        } else if (cur_tok.type == TOKEN_GROUP) {
            advance();
            stmt = parse_group();
            stmt->is_only = 1;
        } else if (cur_tok.type == TOKEN_IMPORT) {
            advance();
            stmt = parse_import();
            stmt->is_only = 1;
        } else if (cur_tok.type == TOKEN_REQUEST) {
            advance();
            ASTNode *n = new_node(AST_LET);
            strcpy(n->value, "__opt__only_req");
            char list[512] = {0};
            int first = 1;
            while (cur_tok.type == TOKEN_IDENT || cur_tok.type == TOKEN_STRING) {
                if (!first) strncat(list, ",", sizeof(list) - strlen(list) - 1);
                strncat(list, cur_tok.value, sizeof(list) - strlen(list) - 1);
                first = 0;
                advance();
                if (!match(TOKEN_COMMA)) break;
            }
            if (first) {
                fprintf(stderr, "[bad] line %d: expected request template name after 'only req'\n", cur_tok.line);
                exit(1);
            }
            ASTNode *sv = new_node(AST_STRING);
            strncpy(sv->value, list, sizeof(sv->value) - 1);
            n->left = sv;
            stmt = n;
        } else {
            fprintf(stderr, "[bad] line %d: expected 'test', 'group', 'import', or 'req' after 'only'\n", cur_tok.line);
            exit(1);
        }
    } else if (cur_tok.type == TOKEN_IMPORT) {
        advance();
        stmt = parse_import();
    } else if (cur_tok.type == TOKEN_LET) {
        advance();
        stmt = parse_let();
    } else if (cur_tok.type == TOKEN_IF) {
        advance();
        stmt = parse_if_stmt();
    } else if (cur_tok.type == TOKEN_FAIL_IF) {
        advance();
        stmt = parse_fail_if();
    } else if (cur_tok.type == TOKEN_PRINT) {
        advance();
        stmt = parse_print();
    } else if (cur_tok.type == TOKEN_STOP) {
        advance();
        stmt = parse_stop_stmt(AST_STOP_ALL, "stop");
    } else if (cur_tok.type == TOKEN_STOP_ALL) {
        advance();
        stmt = parse_stop_stmt(AST_STOP_ALL, "stop_all");
    } else if (cur_tok.type == TOKEN_TIME_START) {
        advance();
        stmt = parse_time_stmt(AST_TIME_START, "time_start");
    } else if (cur_tok.type == TOKEN_TIME_STOP) {
        advance();
        stmt = parse_time_stmt(AST_TIME_STOP, "time_stop");
    } else if (cur_tok.type == TOKEN_SLEEP) {
        advance();
        stmt = parse_sleep_stmt();
    } else if (cur_tok.type == TOKEN_REQUEST) {
        advance();
        stmt = parse_request_template();
    } else if (cur_tok.type == TOKEN_EXPORT) {
        advance();
        stmt = parse_export_decl();
    } else if (cur_tok.type == TOKEN_GROUP) {
        advance();
        stmt = parse_group();
    } else if (cur_tok.type == TOKEN_BEFORE_EACH) {
        advance();
        stmt = parse_before_each();
    } else if (cur_tok.type == TOKEN_BEFORE_ALL) {
        advance();
        stmt = parse_before_all();
    } else if (cur_tok.type == TOKEN_AFTER_EACH) {
        advance();
        stmt = parse_after_each();
    } else if (cur_tok.type == TOKEN_AFTER_ALL) {
        advance();
        stmt = parse_after_all();
    } else if (cur_tok.type == TOKEN_ON_ERROR) {
        advance();
        stmt = parse_hook_block(AST_ON_ERROR, "on_error block", 0);
    } else if (cur_tok.type == TOKEN_ON_ASSERTION_ERROR) {
        advance();
        stmt = parse_hook_block(AST_ON_ASSERTION_ERROR, "on_assertion_error block", 0);
    } else if (cur_tok.type == TOKEN_ON_NETWORK_ERROR) {
        advance();
        stmt = parse_hook_block(AST_ON_NETWORK_ERROR, "on_network_error block", 0);
    } else if (cur_tok.type == TOKEN_BEFORE_URL) {
        advance();
        stmt = parse_hook_block(AST_BEFORE_URL, "before_url block", 1);
    } else if (cur_tok.type == TOKEN_AFTER_URL) {
        advance();
        stmt = parse_hook_block(AST_AFTER_URL, "after_url block", 1);
    } else if (cur_tok.type == TOKEN_ON_URL_ERROR) {
        advance();
        stmt = parse_hook_block(AST_ON_URL_ERROR, "on_url_error block", 1);
    } else if (cur_tok.type == TOKEN_BASE_URL) {
        advance();
        expect(TOKEN_ASSIGN, "base_url assignment");
        ASTNode *n = new_node(AST_LET);
        strcpy(n->value, "__base_url__");
        n->left = parse_value();
        stmt = n;
    } else if (cur_tok.type == TOKEN_TIMEOUT) {
        advance();
        expect(TOKEN_ASSIGN, "timeout assignment");
        ASTNode *n = new_node(AST_LET);
        strcpy(n->value, "__timeout__");
        n->left = parse_value();
        stmt = n;
    } else if (cur_tok.type == TOKEN_RETRY_DELAY_MS) {
        advance();
        expect(TOKEN_ASSIGN, "retry_delay_ms assignment");
        ASTNode *n = new_node(AST_LET);
        strcpy(n->value, "__opt__retry_delay_ms");
        n->left = parse_value();
        stmt = n;
    } else if (cur_tok.type == TOKEN_IDENT) {
        ASTNode *n = new_node(AST_LET);
        char opt_name[512] = {0};
        snprintf(opt_name, sizeof(opt_name), "__opt__%s", cur_tok.value);
        strcpy(n->value, opt_name);
        advance();
        expect(TOKEN_ASSIGN, "top-level option assignment");
        n->left = parse_value();
        stmt = n;
    } else {
        fprintf(stderr,
                "[bad] line %d: unexpected top-level token '%s'\n"
            "      expected one of: test|skip test/group|only test/group|import(use)|let|if|fail_if|sleep|print|stop|stop_all|time_start|time_stop|request(req/template)|export|group|before_all|before_each|after_each|after_all|on_error|on_assertion_error|on_network_error|before_url|after_url|on_url_error|base|timeout|retry_delay_ms|option assignment\n",
                cur_tok.line, cur_tok.value);
        exit(1);
    }

    return stmt;
}

/* ─── top-level program ──────────────────────────────────────── */
ASTNode *parse(void) {
    advance(); /* prime the pump */
    ASTNode *root = new_node(AST_PROGRAM);
    ASTNode *curr = root;

    while (cur_tok.type != TOKEN_EOF) {
        ASTNode *stmt = parse_top_stmt();

        curr->left = stmt;
        if (cur_tok.type != TOKEN_EOF) {
            curr->right = new_node(AST_PROGRAM);
            curr = curr->right;
        }
    }
    return root;
}

/* ─── AST free ───────────────────────────────────────────────── */
void ast_free(ASTNode *n) {
    if (!n) return;
    ast_free(n->left);
    ast_free(n->right);
    ast_free(n->body);
    ast_free(n->headers);
    ast_free(n->stmts);
    ast_free(n->alt);
    ast_free(n->extra);
    ast_free(n->retry);
    ast_free(n->retry_delay);
    ast_free(n->retry_backoff);
    ast_free(n->retry_jitter);
    free(n);
}
