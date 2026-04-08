#ifndef BAD_H
#define BAD_H

#include <stddef.h>

/* ─────────────────────────────────────────────
   TOKEN TYPES
   ───────────────────────────────────────────── */
typedef enum {
    /* Keywords */
    TOKEN_TEST,
    TOKEN_SEND,
    TOKEN_EXPECT,
    TOKEN_LET,
    TOKEN_PRINT,
    TOKEN_IMPORT,
    TOKEN_BODY,
    TOKEN_HEADER,
    TOKEN_STATUS,
    TOKEN_JSON,
    TOKEN_EXISTS,
    TOKEN_BASE_URL,
    TOKEN_TIMEOUT,
    TOKEN_ONLY,
    TOKEN_EXPORT,
    TOKEN_REQUEST,
    TOKEN_AS,
    TOKEN_GROUP,
    TOKEN_BEFORE_ALL,
    TOKEN_BEFORE_EACH,
    TOKEN_AFTER_EACH,
    TOKEN_AFTER_ALL,
    TOKEN_ON_ERROR,
    TOKEN_ON_ASSERTION_ERROR,
    TOKEN_ON_NETWORK_ERROR,
    TOKEN_BEFORE_URL,
    TOKEN_AFTER_URL,
    TOKEN_ON_URL_ERROR,
    TOKEN_SKIP,
    TOKEN_SKIP_IF,
    TOKEN_BECAUSE,
    TOKEN_WITH,
    TOKEN_IF,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,
    TOKEN_ELSE,
    TOKEN_ELSE_IF,
    TOKEN_RETRY,
    TOKEN_RETRY_DELAY_MS,
    TOKEN_STOP,
    TOKEN_STOP_ALL,
    TOKEN_SLEEP,
    TOKEN_FAIL_IF,
    TOKEN_BEARER,
    TOKEN_ENV,
    TOKEN_ARGS,
    TOKEN_TIME_START,
    TOKEN_TIME_STOP,
    TOKEN_TIME,
    TOKEN_TIME_MS,
    TOKEN_NOW_MS,
    /* HTTP methods */
    TOKEN_GET,
    TOKEN_POST,
    TOKEN_PUT,
    TOKEN_PATCH,
    TOKEN_DELETE,
    /* Literals */
    TOKEN_STRING,
    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_BOOL,
    TOKEN_NULL_LIT,
    TOKEN_IDENT,
    /* Punctuation */
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_COLON,
    TOKEN_DOT,
    TOKEN_COMMA,
    /* Operators */
    TOKEN_ASSIGN,
    TOKEN_EQ,
    TOKEN_NEQ,
    TOKEN_LT,
    TOKEN_GT,
    TOKEN_LTE,
    TOKEN_GTE,
    /* Special */
    TOKEN_EOF,
    TOKEN_UNKNOWN
} BadTokenType;

typedef struct {
    BadTokenType type;
    char      value[512];
    int       line;
} Token;

/* ─────────────────────────────────────────────
   LEXER
   ───────────────────────────────────────────── */
void  lexer_init(const char *src);
Token lexer_next(void);
Token lexer_peek(void);

/* ─────────────────────────────────────────────
   AST NODE TYPES
   ───────────────────────────────────────────── */
typedef enum {
    AST_PROGRAM,
    AST_IMPORT,
    AST_TEST,
    AST_SEND,
    AST_EXPECT_STATUS,
    AST_EXPECT_JSON,
    AST_EXPECT_TIME,
    AST_OBJECT,
    AST_LIST,
    AST_COMPARE,
    AST_LET,
    AST_PRINT,
    AST_TIME_START,
    AST_TIME_STOP,
    AST_IF,
    AST_STOP,
    AST_STOP_ALL,
    AST_SKIP_IF,
    AST_FAIL_IF,
    AST_SLEEP,
    AST_REQUEST_TEMPLATE,
    AST_GROUP,
    AST_BEFORE_ALL,
    AST_BEFORE_EACH,
    AST_AFTER_EACH,
    AST_AFTER_ALL,
    AST_ON_ERROR,
    AST_ON_ASSERTION_ERROR,
    AST_ON_NETWORK_ERROR,
    AST_BEFORE_URL,
    AST_AFTER_URL,
    AST_ON_URL_ERROR,
    AST_BODY_PAIR,   /* one key:value inside body{} */
    AST_HEADER_PAIR, /* one key:value inside header{} */
    AST_JSON_PATH,   /* dot-separated path like json.user.id */
    AST_STATUS_REF,  /* last response status value */
    AST_BEARER,      /* "Bearer " + resolved value */
    AST_ENV_REF,     /* environment variable value */
    AST_ARG_REF,     /* CLI arg by index */
    AST_TIME_MS_REF, /* last response time in ms */
    AST_TIMER_REF,   /* elapsed time for a named timer */
    AST_NOW_MS_REF,  /* current epoch time in ms */
    AST_STATS_REF,   /* runtime stats namespace: stats.* */
    AST_STRING,
    AST_INT,
    AST_FLOAT,
    AST_BOOL,
    AST_NULL,
    AST_IDENT,
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType    type;
    char           value[512];   /* string payload, method, path, etc. */
    int            int_val;
    double         float_val;
    int            bool_val;
    struct ASTNode *left;        /* first child / primary expr */
    struct ASTNode *right;       /* next sibling in list */
    struct ASTNode *body;        /* body{} entries */
    struct ASTNode *headers;     /* header{} entries */
    struct ASTNode *stmts;       /* statements in a test block */
    struct ASTNode *alt;         /* else branch / alternate statements */
    struct ASTNode *extra;       /* rhs of expect comparison */
    struct ASTNode *retry;       /* retry expression for send */
    struct ASTNode *retry_delay; /* retry delay expression for send */
    struct ASTNode *retry_backoff; /* retry backoff strategy for send */
    struct ASTNode *retry_jitter;  /* retry jitter milliseconds for send */
    char           op[32];         /* comparison/operator */
    int            is_export;    /* set by `export` declarations */
    int            is_skip;      /* set by `skip test` */
    int            is_only;      /* set by `only test` */
    char           skip_reason[256];
} ASTNode;

ASTNode *parse(void);
void     ast_free(ASTNode *n);

/* ─────────────────────────────────────────────
   HTTP / RUNTIME
   ───────────────────────────────────────────── */
typedef struct {
    int    status;
    char  *body;      /* null-terminated JSON string */
    long   time_ms;
} Response;

/* Variable store */
void        var_set(const char *name, const char *value);
const char *var_get(const char *name);
void        var_clear(void);

/* HTTP engine */
Response http_request(const char *method, const char *url,
                      const char *body_json, /* may be NULL */
                      const char **hdr_keys,
                      const char **hdr_vals,
                      int          hdr_count,
                      int          timeout_ms);
void response_free(Response *r);

/* Output / formatting */
void fmt_print_tree(const char *json, int indent);
void fmt_print_flat(const char *json, const char *prefix);
void fmt_print_table(const char *json);
void fmt_print_json_pretty(const char *json);

/* JSON helpers */
char  *json_get_path(const char *json, const char *dotpath); /* caller frees */
int    json_path_exists(const char *json, const char *dotpath);

/* Config */
typedef struct {
    char base_url[512];
    int  timeout_ms;
    int  pretty_output;
    int  save_history;
    int  print_request;
    int  print_response;
    int  use_color;
    int  fail_fast;
    int  strict_runtime_errors;
    int  save_steps;
    int  remember_token;
    int  show_time;
    int  show_timestamp;
    int  json_view;
    int  json_pretty;
    char log_level[16];
    char history_dir[256];
    char history_file[512];
    char history_format[16];
    char history_mode[16];
    char history_methods[128];
    char history_exclude_methods[128];
    int  history_only_failed;
    int  history_include_headers;
    int  history_include_request_body;
    int  history_include_response_body;
    int  history_max_body_bytes;
} BadConfig;

BadConfig config_load(const char *path); /* .badrc */
BadConfig config_default(void);

typedef struct {
    char base_url[512];
    int  timeout_ms;
    int  timeout_overridden;
    int  base_url_overridden;
    int  verbose;
    int  save_history;
    int  flat_mode;
    int  table_mode;
    int  print_request;
    int  print_response;
    int  use_color;
    int  fail_fast;
    int  strict_runtime_errors;
    int  save_steps;
    int  remember_token;
    int  show_time;
    int  show_timestamp;
    int  json_view;
    int  json_pretty;
    char log_level[16];
    char history_dir[256];
    char history_file[512];
    char history_format[16];
    char history_mode[16];
    char history_methods[128];
    char history_exclude_methods[128];
    int  history_only_failed;
    int  history_include_headers;
    int  history_include_request_body;
    int  history_include_response_body;
    int  history_max_body_bytes;
    char source_file[512];
    char import_only[512];
    int  arg_count;
    char args[32][256];
} RuntimeOptions;

RuntimeOptions runtime_options_default(void);

/* Execute the parsed AST */
int  runtime_exec(ASTNode *root, const RuntimeOptions *opts);

#endif /* BAD_H */
