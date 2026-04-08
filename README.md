# bad — API Testing DSL

A CLI-first API testing language written in C.

BAD helps you write readable API tests with requests, assertions, variables, imports, templates, hooks, and execution controls in a compact syntax.

## Quick Links

- Main binary usage: `./bad file.bad [flags]`
- Full demo: `examples/01-basics/quick_start_demo.bad`
- Examples catalog: `examples/README.md`
- Reusable templates: `examples/02-imports/reusable_templates.bad`
- Group + hook workflow: `examples/03-hooks-and-flow/group_lifecycle_with_overrides.bad`
- Object/template/error-hook workflow: `examples/03-hooks-and-flow/object_template_url_hooks.bad`
- Deterministic regression suite: `examples/03-hooks-and-flow/regression_object_template_hooks.bad`
- Runtime stats demo: `examples/04-runtime/runtime_stats_report.bad`
- Benchmark scenario: `examples/04-runtime/benchmark_baseline.bad`
- Benchmark tool: `node bench/compare.js`
- Config sample: `examples/.badrc`

---

## 1. Install

### macOS

```bash
brew install curl
make
./bad examples/01-basics/quick_start_demo.bad
```

### Linux (Debian/Ubuntu)

```bash
sudo apt update
sudo apt install -y build-essential libcurl4-openssl-dev
make
./bad examples/01-basics/quick_start_demo.bad
```

### Windows (native with MSYS2/MinGW)

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-curl make
make
./bad.exe examples/01-basics/quick_start_demo.bad
```

PowerShell runner:

```powershell
.\run_bad.ps1 examples/01-basics/quick_start_demo.bad
```

### Global install

```bash
sudo make install
bad examples/01-basics/quick_start_demo.bad
```

---

## 2. Fast Start

### Minimal test

```bad
test "ping" {
    send GET "https://jsonplaceholder.typicode.com/users/1"
    expect status 200
}
```

Run it:

```bash
./bad quick.bad
```

### Base URL + short path style

```bad
base_url = "https://jsonplaceholder.typicode.com"
timeout = 10000

test "get user" {
    send GET "/users/1"
    expect status 200
    expect json.id == 1
}
```

---

## 3. Core Syntax

## 3.1 Test blocks

```bad
test "name" {
    send GET "/users/1"
    expect status 200
}
```

## 3.2 Requests (`send`)

Supported methods:

- `GET`
- `POST`
- `PUT`
- `PATCH`
- `DELETE`

Examples:

```bad
send GET "/users/1"
send GET user_path

send POST "/posts" {
    body {
        title: "hello"
        userId: 1
    }
}
```

### Headers and body blocks

```bad
send POST "/posts" {
    body {
        title: "example"
        body: "created by bad"
        userId: 1
    }
    header {
        Content-Type: "application/json"
        Accept: "application/json"
    }
}
```

Aliases also supported:

- `payload` -> `body`
- `headers` -> `header`

## 3.3 Assertions

### Status

```bad
expect status 200
assert status 201
```

### JSON path assertions

```bad
expect json.id exists
expect json.user.name == "krish"
expect json.count > 0
expect json.items.0.price <= 100
expect json.status != "error"
```

Operators:

- `==`
- `!=`
- `>`
- `>=`
- `<`
- `<=`

## 3.4 Variables (`let`)

```bad
let auth_token = "Bearer demo"
let user_path = "/users/1"

send GET user_path {
    header {
        Authorization: auth_token
    }
}
```

### Object variables and spread reuse

Store reusable object fragments and spread them directly into `body` or `header`:

```bad
let common_headers = {
    Accept: "application/json"
    X-Client: "bad"
}

let base_payload = {
    userId: 1
    published: true
}

send POST "/posts" {
    header common_headers
    body {
        base_payload
        title: "from spread"
    }
}
```

`body`/`header` can accept either a block or a direct object source:

```bad
body base_payload
header common_headers
```

### Response capture shortcuts

`let` can now read from the most recent response:

```bad
send POST "/auth/login" {
    body {
        email: "temp@example.com"
        password: "password123"
    }
}

let jwt = json.token
let status_code = status
let auth_header = bearer jwt
let req_ms = time_ms
let now = now_ms
let api_base = env API_BASE
let first_arg = args 0

print jwt
print status_code
```

Notes:

- `json.path` reads from the last response body.
- `status` reads the last response status code.
- `time_ms` reads the last response duration in milliseconds.
- `now_ms` reads current epoch time in milliseconds.
- `time <name>` reads elapsed milliseconds for a named timer.
- `env NAME` reads process environment variable values.
- `args N` reads positional CLI argument at index `N`.
- `bearer <value>` prepends `Bearer ` if missing.
- Object variables are spread-only in request `body`/`header`; using them as scalar values is rejected.
- `print <value>` prints resolved values during test execution.
- Variables are file-scoped at runtime, so values set in one test can be reused in later tests or top-level `print` statements.

### Runtime stats references

BAD exposes built-in runtime metrics through `stats` and `stats.*`.

`stats` is treated as a built-in namespace in value expressions.

Examples:

```bad
print stats
print stats.requests.total
print stats.requests.last_time_ms
print stats.assertions.passed
print stats.runtime.soft_errors

fail_if stats.requests.avg_time_ms > 1200 because "too slow"
```

Supported selectors include:

- `stats.requests.total`
- `stats.requests.successful`
- `stats.requests.network_failures` (alias: `stats.requests.failed`)
- `stats.requests.last_status`
- `stats.requests.last_time_ms`
- `stats.requests.avg_time_ms`
- `stats.requests.total_time_ms`
- `stats.assertions.passed`
- `stats.assertions.failed`
- `stats.assertions.total`
- `stats.assertions.current_test_passed`
- `stats.assertions.current_test_failed`
- `stats.runtime.soft_errors`
- `stats.runtime.zero_assert_tests`
- `stats.runtime.skipped_tests`
- `stats.runtime.skipped_groups`
- `stats.runtime.filtered_tests`
- `stats.runtime.filtered_groups`
- `stats.runtime.strict_runtime_errors`
- `stats.timers.count`

## 3.5 Top-level runtime options

```bad
base_url = "https://jsonplaceholder.typicode.com"
timeout = 10000
print_request = false
print_response = true
show_time = true
show_timestamp = true
strict_runtime_errors = false
json_pretty = true
save_history = true
save_steps = true
history_mode = "per-file"          # all | per-file | per-test | off
history_methods = "GET,POST"       # allow-list by method
history_exclude_methods = "DELETE" # deny-list by method
history_only_failed = false
history_include_headers = true
history_include_request_body = true
history_include_response_body = true
history_max_body_bytes = 0          # 0 => unlimited
history_dir = ".bad-history"
history_file = ".bad-history/all-runs.jsonl"
```

## 3.6 Control Flow And Execution Control

### If / else_if / else

```bad
if json.token exists {
    print "token present"
} else_if status == 429 {
    sleep 100
    stop because "rate limited"
} else {
    stop_all because "token missing"
}
```

Logical operators are supported inside conditions:

```bad
if status == 200 and not json.error exists {
    print "healthy response"
}

if status == 200 or status == 201 {
    print "accepted status"
}

if (status == 200 or status == 201) and not (json.error exists) {
    print "explicitly grouped condition"
}
```

Condition operator precedence:

| Priority | Operator(s) | Notes |
| --- | --- | --- |
| 1 (highest) | `(...)` | explicit grouping |
| 2 | `not` | unary negation |
| 3 | `==`, `!=`, `<`, `<=`, `>`, `>=`, `contains`, `starts_with`, `ends_with`, `regex`, `in`, `exists` | comparison/membership |
| 4 | `and` | logical conjunction |
| 5 (lowest) | `or` | logical disjunction |

Condition grammar (simplified):

```text
condition      := or_expr
or_expr        := and_expr ("or" and_expr)*
and_expr       := unary_expr ("and" unary_expr)*
unary_expr     := "not" unary_expr | "(" condition ")" | primary
primary        := value ["exists" | op value | "in" list]
op             := "==" | "!=" | "<" | "<=" | ">" | ">=" |
                  "contains" | "starts_with" | "ends_with" | "regex"
list           := "[" value ("," value)* "]"
```

Top-level `if/else` is also supported (outside tests). This is useful for suite preflight checks:

```bad
let preflight = send GET "/health"

if status != 200 {
    stop_all because "health check failed"
} else {
    print "preflight ok"
}
```

### Conditional skip in current test

```bad
skip_if status != 200 because "service unavailable"
```

### Conditional hard fail

```bad
fail_if status >= 500 because "server error"
```

### String operators and membership

```bad
expect json.message contains "ready"
expect json.name starts_with "clixiya"
expect json.email ends_with "@example.com"
expect json.trace_id regex "^[a-z0-9-]{8,}$"
expect status in [200, 201, 204]

if status in [200, 201] and not json.error exists {
    print "ok response set"
}
```

### Suite and test pacing

```bad
sleep 250
```

### Stop current test or whole file

```bad
stop because "skip remaining steps in this test"
stop_all because "abort full file execution"
```

### Retry failed requests

Global defaults:

```bad
retry_count = 2
retry_delay_ms = 100
retry_backoff = linear
retry_jitter_ms = 25
```

Per-request override:

```bad
send GET "http://127.0.0.1:9/unreachable" {
    retry 3
    retry_delay_ms 200
    retry_backoff exponential
    retry_jitter_ms 50
}
```

Retry applies to network failures, `429`, and `5xx` statuses.

### Latency assertions

```bad
expect time_ms < 300
expect time auth_flow < 2000
```

### Timing and named timers

Track duration across multiple steps:

```bad
time_start auth_flow
send POST "/auth/login" {
    body {
        email: "temp@example.com"
        password: "password123"
    }
}
time_stop auth_flow

let auth_flow_ms = time auth_flow
print auth_flow_ms
```

Built-in timing values:

- `time_ms`: most recent request duration in milliseconds.
- `time <name>`: elapsed milliseconds for timer `<name>`.
- `now_ms`: current epoch time in milliseconds.
- `<name>_ms`: auto variable written when `time_stop <name>` runs.
- `last_time_ms`: auto variable for most recent request duration.

---

## 4. Imports, Exports, Aliases

## 4.1 Import file

```bad
import "examples/02-imports/shared_exports.bad"
```

## 4.2 Selective import

```bad
import "examples/02-imports/selective_source.bad" only auth_token, api_users
```

## 4.3 Selective import + alias

```bad
import "examples/02-imports/reusable_templates.bad" only profile_path as user_path
```

## 4.4 Export declarations

```bad
export let auth_token = "Bearer xyz"

export request get_user {
    method GET
    path "/users/1"
}
```

## 4.5 Short keyword aliases

- `use` -> `import`
- `assert` -> `expect`
- `req` -> `request`
- `template` -> `request`
- `payload` -> `body`
- `headers` -> `header`
- `base` -> `base_url`
- `wait` -> `timeout`

---

## 5. Request Templates

## 5.1 Define template

```bad
template get_user {
    method GET
    path "/users/1"
    header common_headers
    expect status 200
    expect json.id exists
}
```

Notes:

- `template` is an alias of `request`.
- `path` is optional in template declarations and can be supplied at send time.
- `expect ...` statements inside a template run automatically after the request executes.

## 5.2 Execute template

```bad
test "template call" {
    send req get_user
    expect status 200
}
```

Also valid:

```bad
send request get_user
send template get_user
```

## 5.3 Override template at call site

```bad
send req get_user with {
    path "/users/2"
    payload {
        title: "override"
    }
    headers {
        X-Demo: "1"
    }
}
```

Override behavior:

- `path` override replaces template path.
- Header overrides merge with template headers (`with` values win on duplicate keys).
- Body override replaces template body by default.
- Set `body_merge true` (or `merge_body true`) inside `with { ... }` to merge template body defaults with override fields.

Example body merge:

```bad
send req create_todo with {
    body {
        title: "override title"
    }
    body_merge true
}
```

---

## 6. Groups, Hooks, Skip, Only

## 6.1 Hooks

```bad
before_all {
    print "suite setup"
}

before_each {
    let trace = "start"
}

after_each {
    let trace = "end"
}

after_all {
    let suite = "done"
}
```

### Error hooks

```bad
on_error {
    print "any assertion/network failure"
}

on_assertion_error {
    print "assertion failed"
}

on_network_error {
    print "transport failed"
}
```

### URL wildcard hooks

Pattern matching supports `*` and is evaluated against both full URL and path.

```bad
before_url "/*" {
    print "before request"
}

after_url "/users/*" {
    print "after users request"
}

on_url_error "/users/*" {
    print "users request failed"
}
```

## 6.2 Groups

```bad
group "users" {
    test "get one" {
        send GET "/users/1"
        expect status 200
    }
}
```

## 6.3 Skip with optional reason

```bad
skip test "temporary" because "flaky"

skip group "legacy" because "slow" {
    test "old flow" {
        send GET "/users/9"
        expect status 200
    }
}
```

## 6.4 Only filters

```bad
only test "focus this"
only group "smoke"
only import "examples/02-imports/reusable_templates.bad"
only req load_profile,load_todo
```

Filter behavior summary:

- If any `only test` exists, only those tests run.
- If no `only test` but `only group` exists, only those groups run.
- `only import` limits import execution.
- `only req` runs tests that use selected request templates.

---

## 7. CLI Reference

## 7.1 Basic execution

```bash
./bad file.bad
./bad file.bad --verbose
./bad file.bad --full-trace
./bad file.bad -- arg0 arg1
```

## 7.2 Output formatting

```bash
./bad file.bad --flat
./bad file.bad --table
./bad file.bad --json-view
./bad file.bad --json-pretty
```

## 7.3 Request/response visibility

```bash
./bad file.bad --print-request
./bad file.bad --print-response
./bad file.bad --show-time
./bad file.bad --show-timestamp
```

## 7.4 History capture

```bash
./bad file.bad --save
./bad file.bad --save --save-steps
./bad file.bad --save --save-dir .history
./bad file.bad --save --save-file .bad-history/all-runs.jsonl
./bad file.bad --save --save-mode per-file
./bad file.bad --save --save-mode per-test --save-dir .history
./bad file.bad --save --save-methods GET,POST
./bad file.bad --save --save-exclude-methods DELETE
./bad file.bad --save --save-only-failed
./bad file.bad --save --no-save-response-body --save-max-body-bytes 2048
```

## 7.5 Runtime controls

```bash
./bad file.bad --base https://staging.api.com
./bad file.bad --timeout 3000
./bad file.bad --fail-fast
./bad file.bad --strict-runtime-errors
./bad file.bad --remember-token
./bad file.bad --timing --timestamp
```

## 7.6 Config + logs + color

```bash
./bad file.bad --config examples/.badrc
./bad file.bad --log-level debug --color always
```

---

## 8. Output Modes

### Default

```text
◆ test "login"
    OK status 200
    OK json.token exists
  OK (2/2 passed) [134ms]
```

### Verbose/tree

```text
◆ test "get user"
  response:
  ├─ id: 1
  ├─ name: "krish"
  └─ email: "k@example.com"
```

### Flat (`--flat`)

```text
user.id = 1
user.name = "krish"
```

### Table (`--table`)

```text
id   name
---- --------
1    krish
2    alex
```

---

## 9. Config File (`.badrc`)

Example:

```json
{
  "base_url": "https://api.example.com",
  "timeout": 10000,
  "pretty_output": true,
  "save_history": true,
    "history_mode": "all",
    "history_methods": "",
    "history_exclude_methods": "",
    "history_only_failed": false,
  "save_steps": true,
    "history_include_headers": true,
    "history_include_request_body": true,
    "history_include_response_body": true,
    "history_max_body_bytes": 0,
  "history_dir": ".bad-history",
  "history_file": ".bad-history/all-runs.jsonl",
  "history_format": "jsonl",
  "print_request": false,
  "print_response": false,
    "show_time": false,
    "show_timestamp": false,
  "json_view": false,
  "json_pretty": true,
  "remember_token": false,
  "use_color": true,
  "fail_fast": false,
    "strict_runtime_errors": false,
  "log_level": "info"
}
```

CLI flags override `.badrc` values.

---

## 10. History

With `--save`, BAD writes structured test history.

Records include:

- schema
- id
- timestamp
- source file
- test name
- request snapshot
- response snapshot
- optional step timeline (`--save-steps`)

Advanced save controls:

- `history_mode` / `--save-mode`: `all`, `per-file`, `per-test`, `off`
- `history_methods` / `--save-methods`: allow-list request methods
- `history_exclude_methods` / `--save-exclude-methods`: deny-list methods
- `history_only_failed` / `--save-only-failed`: save only failed tests
- `history_include_headers`: include or omit request headers in history
- `history_include_request_body`: include or omit request body
- `history_include_response_body`: include or omit response body
- `history_max_body_bytes` / `--save-max-body-bytes`: truncate stored bodies

`--save-file` still appends to one JSONL file in `all` mode. Use `per-file` or `per-test` mode when you want automatic split files under `history_dir`.

---

## 11. Example Files

The `examples/` directory is bundled and ready to run.

Start here:

- `examples/01-basics/quick_start_demo.bad` for first run
- `examples/04-runtime/advanced_runtime_controls.bad` for grouped conditions, string operators, `in`, retry backoff/jitter, env/args, timers
- `examples/02-imports/composed_import_suite.bad` for import-driven suites
- `examples/03-hooks-and-flow/object_template_url_hooks.bad` for object spread vars, template inline expects, and URL/error hooks
- `examples/03-hooks-and-flow/regression_object_template_hooks.bad` for deterministic regression of object/template/hook behavior
- `examples/04-runtime/runtime_stats_report.bad` for built-in runtime metrics via `stats.*`
- `examples/04-runtime/benchmark_baseline.bad` for benchmark runs

Full index and coverage map: `examples/README.md`

---

## 12. Keyword Reference

## 12.1 Primary keywords

- `test`
- `send`
- `expect`
- `let`
- `print`
- `import`
- `export`
- `request`
- `template`
- `group`
- `before_all`
- `before_each`
- `after_each`
- `after_all`
- `on_error`
- `on_assertion_error`
- `on_network_error`
- `before_url`
- `after_url`
- `on_url_error`
- `skip`
- `skip_if`
- `fail_if`
- `only`
- `because`
- `with`
- `if`
- `and`
- `or`
- `not`
- `else`
- `else_if`
- `contains`
- `starts_with`
- `ends_with`
- `regex`
- `in`
- `retry`
- `retry_delay_ms`
- `retry_backoff`
- `retry_jitter_ms`
- `sleep`
- `stop`
- `stop_all`
- `bearer`
- `env`
- `args`
- `time_start`
- `time_stop`
- `time`
- `time_ms`
- `now_ms`

## 12.2 Assertion terms

- `status`
- `json`
- `exists`
- `contains`
- `starts_with`
- `ends_with`
- `regex`
- `in`

## 12.3 Structural terms

- `body` / `payload`
- `header` / `headers`

## 12.4 Config aliases

- `base` / `base_url`
- `wait` / `timeout`

## 12.5 Usage Cheat Sheet

- `test`: `test "health" { ... }`
- `send`: `send GET "/users/1"`
- `expect` / `assert`: `expect status 200`
- `let`: `let token = json.token`
- `print`: `print token`
- `import` / `use`: `import "examples/02-imports/shared_exports.bad"`
- `export`: `export let profile_path = "/users/1"`
- `request` / `req`: `request get_user { method GET path "/users/1" }`
- `template`: `template get_user { method GET path "/users/1" }`
- `group`: `group "users" { ... }`
- `before_all`: `before_all { print "suite setup" }`
- `before_each`: `before_each { let trace = "start" }`
- `after_each`: `after_each { let trace = "end" }`
- `after_all`: `after_all { print "suite done" }`
- `on_error`: `on_error { print "failure" }`
- `on_assertion_error`: `on_assertion_error { print "assert failed" }`
- `on_network_error`: `on_network_error { print "network failed" }`
- `before_url`: `before_url "/*" { print "before" }`
- `after_url`: `after_url "/users/*" { print "after" }`
- `on_url_error`: `on_url_error "/users/*" { print "url failed" }`
- `skip`: `skip test "legacy" because "flaky"`
- `skip_if`: `skip_if status != 200 because "service down"`
- `fail_if`: `fail_if status >= 500 because "server error"`
- `only`: `only test "smoke"`
- `because`: `stop because "precondition failed"`
- `with`: `send req get_user with { path "/users/2" }`
- `body_merge` / `merge_body`: `send req create_todo with { body { title: "x" } body_merge true }`
- `if`: `if status == 200 { ... }`
- `else_if`: `else_if status == 429 { sleep 100 }`
- `else`: `else { stop_all because "abort" }`
- `and`: `if status == 200 and json.ok exists { ... }`
- `or`: `if status == 200 or status == 201 { ... }`
- `not`: `if not json.error exists { ... }`
- Parentheses grouping: `if (status == 200 or status == 201) and not json.error exists { ... }`
- `contains`: `expect json.message contains "ready"`
- `starts_with`: `expect json.name starts_with "clixiya"`
- `ends_with`: `expect json.email ends_with "@example.com"`
- `regex`: `expect json.trace_id regex "^[a-z0-9-]+$"`
- `in`: `expect status in [200, 201, 204]`
- `retry`: `send GET "/foo" { retry 3 }`
- `retry_delay_ms`: `send GET "/foo" { retry_delay_ms 200 }`
- `retry_backoff`: `send GET "/foo" { retry_backoff exponential }`
- `retry_jitter_ms`: `send GET "/foo" { retry_jitter_ms 50 }`
- `sleep`: `sleep 250`
- `stop`: `stop because "end this test"`
- `stop_all`: `stop_all because "stop suite"`
- `bearer`: `let auth = bearer token`
- `env`: `let api_base = env API_BASE`
- `args`: `let first = args 0`
- `time_start`: `time_start auth_flow`
- `time_stop`: `time_stop auth_flow`
- `time`: `expect time auth_flow < 2000`
- `time_ms`: `expect time_ms < 300`
- `now_ms`: `expect now_ms > 0`
- `status`: `expect status >= 200`
- `json`: `expect json.user.id == 1`
- `exists`: `expect json.user.name exists`
- `body` / `payload`: `payload { title: "hello" }`
- `header` / `headers`: `headers { Accept: "application/json" }`
- `base` / `base_url`: `base = "https://api.example.com"`
- `wait` / `timeout`: `wait = 10000`

---

## 13. Project Structure

```text
bad/
├── bench/
│   ├── compare.js
│   └── results/
├── examples/
│   ├── README.md
│   ├── demo.bad
│   ├── advanced_features.bad
│   └── benchmark.bad
├── include/
│   └── bad.h
│   └── bad_platform.h
├── src/
│   ├── main.c
│   ├── lexer.c
│   ├── parser.c
│   ├── runtime.c
│   ├── http.c
│   ├── json_helpers.c
│   └── vars.c
├── run_bad.sh
├── run_bad.ps1
├── run_bad.cmd
├── extenstion/
└── server/
```

---

## 14. Troubleshooting

## 14.1 Build failures

```bash
make clean && make
```

Ensure curl development libraries are installed.

## 14.2 Timeout errors

Increase timeout in file or CLI:

```bad
timeout = 30000
```

or

```bash
./bad file.bad --timeout 30000
```

## 14.3 Import not found

Use correct relative path from current working directory.

## 14.4 Assertion path not found

Use `--print-response` or `--json-pretty` to inspect shape.

---

## 15. Exit Codes

- `0` => all assertions passed
- `1` => one or more assertions failed

---

## 16. FAQ

### Is `send request` same as `send req`?
Yes.

### Can I use a variable as a request path?
Yes, e.g. `send GET user_path`.

### Can I skip a test without a block?
Yes, `skip test "name" because "reason"`.

### Can I run with local server instead of public API?
Yes, set `base_url` to local server address.

---

## 17. Command Cookbook

```bash
# simple run
./bad examples/01-basics/quick_start_demo.bad

# config run
./bad examples/01-basics/quick_start_demo.bad --config examples/.badrc

# debug request and response
./bad examples/01-basics/quick_start_demo.bad --print-request --print-response

# include timing diagnostics
./bad examples/01-basics/quick_start_demo.bad --show-time --show-timestamp

# pretty output + history
./bad examples/01-basics/quick_start_demo.bad --json-pretty --save --save-steps

# fail-fast CI style
./bad examples/01-basics/quick_start_demo.bad --fail-fast --color never
```

---

## 18. Contributor Checklist

1. Update parser/runtime features.
2. Add or update examples.
3. Run full example suite.
4. Update docs (`README.md`, extension docs).
5. Rebuild extension VSIX if language behavior changed.

---

## 19. Benchmarks

Run the benchmark comparison:

```bash
node bench/compare.js
```

Latest checked-in results: `bench/results/latest.md`

Current snapshot (process startup included per run):

| Tool | Runs | Mean | Median | P95 | Min | Max |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| bad | 15 | 361.77 ms | 371.66 ms | 436.30 ms | 282.59 ms | 436.30 ms |
| curl | 15 | 355.22 ms | 350.45 ms | 450.34 ms | 280.03 ms | 450.34 ms |

Interpretation:

- BAD is within a close range of raw `curl` for this public endpoint.
- Numbers vary with network jitter; rerun locally for decision-grade comparisons.

---

End of main README.
