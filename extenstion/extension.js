const path = require("path");
const vscode = require("vscode");

const METHODS = ["GET", "POST", "PUT", "PATCH", "DELETE"];

const KEYWORDS = [
  "test", "send", "expect", "assert", "let", "use", "import", "body", "payload", "header", "headers",
  "status", "json", "exists", "base", "base_url", "wait", "timeout", "only", "export", "request", "req", "template",
  "method", "path",
  "as", "group", "before_all", "before_each", "after_each", "after_all", "skip", "skip_if", "fail_if", "because", "with",
  "body_merge", "merge_body", "if", "and", "or", "not", "else", "else_if", "contains", "starts_with", "ends_with", "regex",
  "in", "retry", "retry_count", "retry_delay_ms", "retry_backoff", "retry_jitter_ms", "stop", "stop_all", "sleep", "print", "bearer", "env",
  "args", "time_start", "time_stop", "time", "time_ms", "now_ms", "stats", "on_error", "on_assertion_error", "on_network_error",
  "before_url", "after_url", "on_url_error", "strict_runtime",
  "history_mode", "history_methods", "history_exclude_methods", "history_only_failed",
  "history_include_headers", "history_include_request_body", "history_include_response_body", "history_max_body_bytes",
  "save_mode", "save_methods", "save_exclude_methods", "save_only_failed",
  "save_headers", "save_request_body", "save_response_body", "save_max_body_bytes"
];

const TOP_LEVEL_OPTIONS = [
  "base",
  "base_url",
  "wait",
  "timeout",
  "retry_count",
  "retry_delay_ms",
  "retry_backoff",
  "retry_jitter_ms",
  "save_history",
  "save_steps",
  "print_request",
  "print_response",
  "show_time",
  "show_timestamp",
  "json_view",
  "json_pretty",
  "flat",
  "table",
  "fail_fast",
  "strict_runtime_errors",
  "strict_runtime",
  "remember_token",
  "use_color",
  "history_dir",
  "history_file",
  "history_format",
  "history_mode",
  "history_methods",
  "history_exclude_methods",
  "history_only_failed",
  "history_include_headers",
  "history_include_request_body",
  "history_include_response_body",
  "history_max_body_bytes",
  "save_mode",
  "save_methods",
  "save_exclude_methods",
  "save_only_failed",
  "save_headers",
  "save_request_body",
  "save_response_body",
  "save_max_body_bytes",
  "log_level",
  "only_req",
  "only_import"
];

const STATS_SELECTORS = [
  "requests.total",
  "requests.successful",
  "requests.failed",
  "requests.network_failures",
  "requests.last_status",
  "requests.last_time_ms",
  "requests.avg_time_ms",
  "requests.total_time_ms",
  "assertions.passed",
  "assertions.failed",
  "assertions.total",
  "assertions.current_test_passed",
  "assertions.current_test_failed",
  "runtime.soft_errors",
  "runtime.zero_assert_tests",
  "runtime.skipped_tests",
  "runtime.skipped_groups",
  "runtime.filtered_tests",
  "runtime.filtered_groups",
  "runtime.strict_runtime_errors",
  "timers.count"
];

const HOVER_DOCS = {
  test: {
    summary: "Defines an executable test block.",
    syntax: 'test "name" {\n  send GET "/health"\n  expect status 200\n}',
    example: 'test "health" {\n  send GET "/health"\n  expect status 200\n}',
    related: ["group", "send", "expect", "fail_if"]
  },
  group: {
    summary: "Groups tests and local hooks into a reusable scope.",
    syntax: 'group "suite" {\n  before_each {\n    print "setup"\n  }\n}',
    example: 'group "users" {\n  test "list" {\n    send GET "/users"\n  }\n}',
    related: ["test", "before_each", "after_each", "only", "skip"]
  },
  request: {
    summary: "Declares a reusable request template.",
    syntax: 'request get_user {\n  method GET\n  path "/users/1"\n}',
    example: "send req get_user",
    related: ["template", "req", "send", "with"]
  },
  template: {
    summary: "Alias keyword for request templates.",
    syntax: 'template create_user {\n  method POST\n  path "/users"\n}',
    example: 'send template create_user with {\n  body { name: "clixiya" }\n}',
    related: ["request", "req", "with", "body_merge"]
  },
  method: {
    summary: "Defines HTTP method inside a request/template declaration.",
    syntax: 'request list_users {\n  method GET\n  path "/users"\n}',
    example: 'template create_user {\n  method POST\n  path "/users"\n}',
    related: ["request", "template", "path", "send"]
  },
  path: {
    summary: "Defines request URL path inside a request/template declaration.",
    syntax: 'request get_user {\n  method GET\n  path "/users/1"\n}',
    example: 'send req get_user with {\n  path "/users/2"\n}',
    related: ["request", "template", "method", "send"]
  },
  send: {
    summary: "Sends an HTTP request directly or via template.",
    syntax: 'send GET "/path"\n# or\nsend req template_name with { path "/override" }',
    example: 'send POST "/posts" {\n  body { title: "hello" }\n}',
    related: ["expect", "request", "template", "retry", "with"]
  },
  expect: {
    summary: "Adds an assertion for status/json/time values.",
    syntax: 'expect status 200\nexpect status >= 200\nexpect json.user.id exists',
    example: 'expect json.name contains "clixiya"',
    related: ["assert", "status", "json", "time_ms"]
  },
  assert: {
    summary: "Alias of expect.",
    syntax: "assert status in [200, 201]",
    example: "assert json.token exists",
    related: ["expect", "status", "json"]
  },
  skip_if: {
    summary: "Skips remaining statements when condition is true.",
    syntax: 'skip_if status != 200 because "service unavailable"',
    example: 'skip_if stats.requests.failed > 0 because "network unstable"',
    related: ["fail_if", "if", "because"]
  },
  fail_if: {
    summary: "Fails immediately when condition is true.",
    syntax: 'fail_if status >= 500 because "server failure"',
    example: 'fail_if stats.runtime.soft_errors > 0 because "runtime issue"',
    related: ["skip_if", "if", "because"]
  },
  with: {
    summary: "Applies inline overrides to send req/template.",
    syntax: 'send req t with {\n  path "/override"\n  body_merge true\n}',
    example: 'send template create_todo with {\n  body { title: "override" }\n}',
    related: ["send", "request", "template", "body_merge", "merge_body"]
  },
  body_merge: {
    summary: "Merges template body defaults with override body fields.",
    syntax: "body_merge true",
    example: "merge_body true",
    related: ["merge_body", "with", "template"]
  },
  merge_body: {
    summary: "Alias of body_merge.",
    syntax: "merge_body true",
    example: "body_merge true",
    related: ["body_merge", "with"]
  },
  on_error: {
    summary: "Runs on assertion or network errors.",
    syntax: 'on_error {\n  print "error"\n}',
    example: "on_error { print stats.runtime.soft_errors }",
    related: ["on_assertion_error", "on_network_error", "on_url_error"]
  },
  on_assertion_error: {
    summary: "Runs only on assertion failures.",
    syntax: 'on_assertion_error {\n  print "assert fail"\n}',
    example: "on_assertion_error { print stats.assertions.failed }",
    related: ["on_error", "expect", "assert"]
  },
  on_network_error: {
    summary: "Runs on network/transport failures.",
    syntax: 'on_network_error {\n  print "network fail"\n}',
    example: "on_network_error { print stats.requests.network_failures }",
    related: ["on_error", "retry", "on_url_error"]
  },
  before_url: {
    summary: "Runs before matching URL/path calls.",
    syntax: 'before_url "/users/*" {\n  print "before"\n}',
    example: 'before_url "/*" { time_start flow }',
    related: ["after_url", "on_url_error"]
  },
  after_url: {
    summary: "Runs after matching URL/path calls.",
    syntax: 'after_url "/users/*" {\n  print "after"\n}',
    example: 'after_url "/*" { time_stop flow }',
    related: ["before_url", "on_url_error"]
  },
  on_url_error: {
    summary: "Runs on matching URL/path failures.",
    syntax: 'on_url_error "/unreachable*" {\n  print "url error"\n}',
    example: 'on_url_error "/*" { print status }',
    related: ["on_error", "before_url", "after_url"]
  },
  print: {
    summary: "Prints resolved runtime values.",
    syntax: "print stats.requests.total",
    example: "print json.id",
    related: ["stats", "json", "status", "time_ms"]
  },
  status: {
    summary: "References last response status.",
    syntax: "expect status >= 200",
    example: 'fail_if status >= 500 because "server failure"',
    related: ["expect", "assert", "fail_if"]
  },
  json: {
    summary: "References last response JSON path.",
    syntax: "expect json.user.name exists",
    example: "let user_id = json.user.id",
    related: ["expect", "let", "exists"]
  },
  retry: {
    summary: "Retries requests on transient failures.",
    syntax: "retry 2",
    example: "retry_delay_ms 100",
    related: ["retry_delay_ms", "retry_backoff", "retry_jitter_ms"]
  },
  time_start: {
    summary: "Starts a named timer.",
    syntax: "time_start flow_name",
    example: "time_stop flow_name",
    related: ["time_stop", "time", "time_ms"]
  },
  time_stop: {
    summary: "Stops a named timer and stores <name>_ms.",
    syntax: "time_stop flow_name",
    example: "print time flow_name",
    related: ["time_start", "time", "time_ms"]
  },
  time: {
    summary: "Resolves elapsed ms for a named timer.",
    syntax: "print time flow_name",
    example: 'fail_if time flow_name > 1000 because "slow"',
    related: ["time_start", "time_stop", "time_ms"]
  },
  time_ms: {
    summary: "Resolves duration of last response in ms.",
    syntax: "expect time_ms < 500",
    example: "let last = time_ms",
    related: ["expect", "time", "fail_if"]
  },
  now_ms: {
    summary: "Resolves current epoch timestamp in ms.",
    syntax: "let ts = now_ms",
    example: 'fail_if now_ms < 0 because "invalid"',
    related: ["let", "time_ms"]
  },
  stats: {
    summary: "Built-in runtime statistics namespace.",
    syntax: "print stats\nprint stats.requests.total",
    example: 'fail_if stats.runtime.soft_errors > 0 because "runtime issue"',
    related: ["print", "fail_if", "expect", "time_ms"]
  },
  only_req: {
    summary: "Top-level option to run only selected request templates.",
    syntax: 'only_req = "login,profile"',
    example: 'only_req = "create_user"',
    related: ["request", "template", "only", "only_import"]
  },
  only_import: {
    summary: "Top-level option to run only imported scope by alias/module.",
    syntax: 'only_import = "shared"',
    example: 'only_import = "auth"',
    related: ["import", "use", "only", "only_req"]
  },
  strict_runtime_errors: {
    summary: "Promotes runtime soft errors to hard failures.",
    syntax: "strict_runtime_errors = true",
    example: "strict_runtime = true",
    related: ["fail_fast", "stats", "runtime"]
  },
  history_mode: {
    summary: "Controls how history files are written.",
    syntax: 'history_mode = "all"  # all | per-file | per-test | off',
    example: 'history_mode = "per-file"',
    related: ["save_history", "history_file", "history_dir", "history_methods"]
  },
  history_methods: {
    summary: "Comma-separated allow-list of HTTP methods to save.",
    syntax: 'history_methods = "GET,POST"',
    example: 'history_methods = "GET"',
    related: ["history_exclude_methods", "history_mode", "save_history"]
  },
  history_exclude_methods: {
    summary: "Comma-separated deny-list of HTTP methods to skip in history.",
    syntax: 'history_exclude_methods = "DELETE"',
    example: 'history_exclude_methods = "POST,PATCH"',
    related: ["history_methods", "history_mode", "save_history"]
  },
  history_only_failed: {
    summary: "Saves history entries only when a test fails.",
    syntax: "history_only_failed = true",
    example: "history_only_failed = true",
    related: ["save_history", "fail_fast", "history_mode"]
  },
  history_max_body_bytes: {
    summary: "Limits stored request/response body bytes per history record.",
    syntax: "history_max_body_bytes = 2048",
    example: "history_max_body_bytes = 0  # unlimited",
    related: ["history_include_request_body", "history_include_response_body", "save_history"]
  }
};

const SETTINGS_FIELDS = [
  { section: "Run", key: "defaultConfigPath", type: "string", defaultValue: "examples/.badrc", label: "Default Config Path" },
  { section: "Run", key: "runBinaryPath", type: "string", defaultValue: "./bad", label: "Run Binary Path" },
  { section: "Run", key: "runExtraArgs", type: "array", defaultValue: [], label: "Run Extra Args (JSON array)" },
  { section: "Run", key: "runUseConfigByDefault", type: "boolean", defaultValue: false, label: "Run Uses Config By Default" },

  { section: "Diagnostics", key: "enableDiagnostics", type: "boolean", defaultValue: true, label: "Enable Diagnostics" },
  { section: "Diagnostics", key: "diagnosticsTopLevelHeuristics", type: "boolean", defaultValue: true, label: "Warn Unknown Top-Level Statements" },
  { section: "Diagnostics", key: "diagnosticsWarnNotAmbiguousNot", type: "boolean", defaultValue: true, label: "Warn Ambiguous not a == b" },

  { section: "Formatting", key: "enableFormatting", type: "boolean", defaultValue: true, label: "Enable Formatting" },
  { section: "Formatting", key: "formatterIndentSize", type: "number", defaultValue: 2, label: "Indent Size" },
  { section: "Formatting", key: "formatterWrapColumn", type: "number", defaultValue: 100, label: "Wrap Column" },
  { section: "Formatting", key: "formatterWrapLogicalConditions", type: "boolean", defaultValue: true, label: "Wrap Logical Conditions" },

  { section: "Editor", key: "enableCompletions", type: "boolean", defaultValue: true, label: "Enable Completions" },
  { section: "Editor", key: "enableHovers", type: "boolean", defaultValue: true, label: "Enable Hover Docs" },
  { section: "Editor", key: "hoverShowExamples", type: "boolean", defaultValue: true, label: "Hover Shows Examples" },
  { section: "Editor", key: "hoverShowRelated", type: "boolean", defaultValue: true, label: "Hover Shows Related Keywords" },
  { section: "Editor", key: "hoverMaxRelated", type: "number", defaultValue: 5, label: "Hover Max Related Keywords" },
  { section: "Editor", key: "enableSignatures", type: "boolean", defaultValue: true, label: "Enable Signature Help" },
  { section: "Editor", key: "enableSymbols", type: "boolean", defaultValue: true, label: "Enable Outline Symbols" },
  { section: "Editor", key: "enableSemanticTokens", type: "boolean", defaultValue: true, label: "Enable Semantic Tokens" },
  { section: "Editor", key: "enableCodeLens", type: "boolean", defaultValue: true, label: "Enable CodeLens" },
  { section: "Editor", key: "enableImportLinks", type: "boolean", defaultValue: true, label: "Enable Import Links" },

  { section: "Visual", key: "showFileDecorations", type: "boolean", defaultValue: true, label: "Show File Decorations" },
  { section: "Visual", key: "sidebarShowRunActions", type: "boolean", defaultValue: true, label: "Sidebar Shows Run Actions" },
  { section: "Visual", key: "sidebarShowDocs", type: "boolean", defaultValue: true, label: "Sidebar Shows Docs Action" }
];

const RUNTIME_CONFIG_FIELDS = [
  { section: "Core", key: "base_url", type: "string", defaultValue: "", label: "Base URL" },
  { section: "Core", key: "timeout", type: "number", defaultValue: 10000, label: "Timeout (ms)" },
  { section: "Core", key: "save_history", type: "boolean", defaultValue: false, label: "Save History" },
  { section: "Core", key: "save_steps", type: "boolean", defaultValue: false, label: "Save Steps" },

  { section: "History", key: "history_mode", type: "string", defaultValue: "all", label: "History Mode" },
  { section: "History", key: "history_dir", type: "string", defaultValue: ".bad-history", label: "History Directory" },
  { section: "History", key: "history_file", type: "string", defaultValue: ".bad-history/all-runs.jsonl", label: "History File" },
  { section: "History", key: "history_format", type: "string", defaultValue: "jsonl", label: "History Format" },
  { section: "History", key: "history_methods", type: "string", defaultValue: "", label: "Save Methods (CSV)" },
  { section: "History", key: "history_exclude_methods", type: "string", defaultValue: "", label: "Exclude Methods (CSV)" },
  { section: "History", key: "history_only_failed", type: "boolean", defaultValue: false, label: "Save Only Failed Tests" },
  { section: "History", key: "history_include_headers", type: "boolean", defaultValue: true, label: "Include Headers" },
  { section: "History", key: "history_include_request_body", type: "boolean", defaultValue: true, label: "Include Request Body" },
  { section: "History", key: "history_include_response_body", type: "boolean", defaultValue: true, label: "Include Response Body" },
  { section: "History", key: "history_max_body_bytes", type: "number", defaultValue: 0, label: "Max Body Bytes (0 = unlimited)" },

  { section: "Output", key: "print_request", type: "boolean", defaultValue: false, label: "Print Request" },
  { section: "Output", key: "print_response", type: "boolean", defaultValue: false, label: "Print Response" },
  { section: "Output", key: "show_time", type: "boolean", defaultValue: false, label: "Show Time" },
  { section: "Output", key: "show_timestamp", type: "boolean", defaultValue: false, label: "Show Timestamp" },
  { section: "Output", key: "json_view", type: "boolean", defaultValue: false, label: "JSON View" },
  { section: "Output", key: "json_pretty", type: "boolean", defaultValue: true, label: "JSON Pretty" },
  { section: "Output", key: "use_color", type: "boolean", defaultValue: true, label: "Use Color" },
  { section: "Output", key: "log_level", type: "string", defaultValue: "info", label: "Log Level" },

  { section: "Behavior", key: "remember_token", type: "boolean", defaultValue: false, label: "Remember Token" },
  { section: "Behavior", key: "fail_fast", type: "boolean", defaultValue: false, label: "Fail Fast" },
  { section: "Behavior", key: "strict_runtime_errors", type: "boolean", defaultValue: false, label: "Strict Runtime Errors" },
  { section: "Behavior", key: "only_req", type: "string", defaultValue: "", label: "Only Request Templates" },
  { section: "Behavior", key: "only_import", type: "string", defaultValue: "", label: "Only Imports" }
];

function runtimeConfigDefaults() {
  const out = {};
  for (const field of RUNTIME_CONFIG_FIELDS) {
    out[field.key] = field.defaultValue;
  }
  return out;
}

function normalizeRuntimeConfigValues(input) {
  const source = input && typeof input === "object" ? input : {};
  const out = {};

  for (const field of RUNTIME_CONFIG_FIELDS) {
    let value = Object.prototype.hasOwnProperty.call(source, field.key)
      ? source[field.key]
      : field.defaultValue;

    if (field.type === "boolean") {
      value = !!value;
    } else if (field.type === "number") {
      const num = Number(value);
      value = Number.isFinite(num) ? num : field.defaultValue;
      if (value < 0 && field.key === "history_max_body_bytes") value = 0;
      if (value <= 0 && field.key === "timeout") value = field.defaultValue;
    } else {
      value = value == null ? "" : String(value);
    }

    out[field.key] = value;
  }

  return out;
}

function cfg() {
  return vscode.workspace.getConfiguration("badLanguage");
}

function setting(key, fallback) {
  return cfg().get(key, fallback);
}

function settingBool(key, fallback) {
  return !!setting(key, fallback);
}

function settingNum(key, fallback) {
  const num = Number(setting(key, fallback));
  return Number.isFinite(num) ? num : fallback;
}

function isBadDocument(document) {
  return !!document && document.languageId === "bad";
}

function isBadrcUri(uri) {
  return /(^|\/)\.badrc$/.test(uri.path || "");
}

function shellQuote(value) {
  const raw = String(value ?? "");
  if (process.platform === "win32") {
    return `"${raw.replace(/"/g, '""')}"`;
  }
  return `"${raw.replace(/\\/g, "\\\\").replace(/"/g, '\\"')}"`;
}

function formatShellArg(value) {
  const raw = String(value ?? "");
  return /^[A-Za-z0-9_./:=+\-]+$/.test(raw) ? raw : shellQuote(raw);
}

class SidebarActionItem extends vscode.TreeItem {
  constructor(label, description, commandId, iconId) {
    super(label, vscode.TreeItemCollapsibleState.None);
    this.description = description;
    this.command = { command: commandId, title: label };
    this.iconPath = new vscode.ThemeIcon(iconId);
    this.contextValue = "badSidebar.action";
  }
}

class BadSidebarProvider {
  constructor() {
    this.emitter = new vscode.EventEmitter();
    this.onDidChangeTreeData = this.emitter.event;
  }

  refresh() {
    this.emitter.fire(undefined);
  }

  getTreeItem(item) {
    return item;
  }

  getChildren(item) {
    if (item) return [];

    const items = [];
    if (settingBool("sidebarShowRunActions", true)) {
      items.push(new SidebarActionItem("Run Current File", "Uses BAD run settings", "badLanguage.runCurrentFileAuto", "play"));
      items.push(new SidebarActionItem("Run With Config", "Forces --config", "badLanguage.runCurrentFileWithConfig", "tools"));
    }

    items.push(new SidebarActionItem("Runtime Config", "Edit .badrc in webview", "badLanguage.openRuntimeConfigPanel", "file-code"));
    items.push(new SidebarActionItem("Open BAD Settings", "Customize extension behavior", "badLanguage.openSettingsPanel", "settings-gear"));
    items.push(new SidebarActionItem("Create examples/.badrc", "Generate local config sample", "badLanguage.createExampleBadrc", "new-file"));
    items.push(new SidebarActionItem("Use BAD Icons", "Switch file icon theme", "badLanguage.useBadIconTheme", "symbol-color"));

    if (settingBool("sidebarShowDocs", true)) {
      items.push(new SidebarActionItem("Open README", "Open extension docs", "badLanguage.openExtensionReadme", "book"));
    }

    if (items.length === 0) {
      const empty = new vscode.TreeItem("No sidebar actions enabled", vscode.TreeItemCollapsibleState.None);
      empty.description = "Enable actions in settings";
      empty.iconPath = new vscode.ThemeIcon("eye-closed");
      empty.command = { command: "badLanguage.openSettingsPanel", title: "Open BAD Settings" };
      return [empty];
    }

    return items;
  }
}

function activate(context) {
  const diagnostics = vscode.languages.createDiagnosticCollection("bad");
  context.subscriptions.push(diagnostics);

  const sidebarProvider = new BadSidebarProvider();
  context.subscriptions.push(
    vscode.window.createTreeView("badSidebar.quickActions", {
      treeDataProvider: sidebarProvider,
      showCollapseAll: false
    })
  );

  const decorationProvider = createDecorationProvider();

  context.subscriptions.push(
    vscode.commands.registerCommand("badLanguage.runCurrentFile", () => runCurrentBadFile("plain")),
    vscode.commands.registerCommand("badLanguage.runCurrentFileWithConfig", () => runCurrentBadFile("config")),
    vscode.commands.registerCommand("badLanguage.runCurrentFileAuto", () => runCurrentBadFile("auto")),
    vscode.commands.registerCommand("badLanguage.openRuntimeConfigPanel", () => openRuntimeConfigPanel(context)),
    vscode.commands.registerCommand("badLanguage.openSettingsPanel", () => openSettingsPanel(context)),
    vscode.commands.registerCommand("badLanguage.createExampleBadrc", () => createExampleBadrc()),
    vscode.commands.registerCommand("badLanguage.useBadIconTheme", () => useBadIconTheme()),
    vscode.commands.registerCommand("badLanguage.openExtensionReadme", () => openExtensionReadme(context)),
    vscode.commands.registerCommand("badLanguage.refreshSidebar", () => sidebarProvider.refresh())
  );

  context.subscriptions.push(
    vscode.languages.registerCompletionItemProvider(
      { language: "bad" },
      createCompletionProvider(),
      " ",
      "/",
      ".",
      '"',
      ":"
    )
  );

  context.subscriptions.push(
    vscode.languages.registerHoverProvider({ language: "bad" }, createHoverProvider())
  );

  context.subscriptions.push(
    vscode.languages.registerSignatureHelpProvider(
      { language: "bad" },
      createSignatureProvider(),
      " "
    )
  );

  context.subscriptions.push(
    vscode.languages.registerDocumentSymbolProvider({ language: "bad" }, createSymbolProvider())
  );

  context.subscriptions.push(
    vscode.languages.registerDocumentFormattingEditProvider({ language: "bad" }, createFormattingProvider())
  );

  context.subscriptions.push(
    vscode.languages.registerCodeLensProvider({ language: "bad" }, createCodeLensProvider())
  );

  context.subscriptions.push(
    vscode.languages.registerDocumentLinkProvider({ language: "bad" }, createImportLinkProvider())
  );

  const legend = new vscode.SemanticTokensLegend(
    ["keyword", "string", "number", "operator", "variable"],
    ["declaration"]
  );

  context.subscriptions.push(
    vscode.languages.registerDocumentSemanticTokensProvider(
      { language: "bad" },
      createSemanticProvider(legend),
      legend
    )
  );

  context.subscriptions.push(
    vscode.window.registerFileDecorationProvider(decorationProvider)
  );

  const updateDiagnosticsForDocument = (document) => {
    if (!isBadDocument(document)) return;
    if (!settingBool("enableDiagnostics", true)) {
      diagnostics.delete(document.uri);
      return;
    }
    diagnostics.set(document.uri, collectDiagnostics(document));
  };

  const refreshAllDiagnostics = () => {
    for (const document of vscode.workspace.textDocuments) {
      updateDiagnosticsForDocument(document);
    }
  };

  context.subscriptions.push(
    vscode.workspace.onDidOpenTextDocument(updateDiagnosticsForDocument),
    vscode.workspace.onDidChangeTextDocument((event) => updateDiagnosticsForDocument(event.document)),
    vscode.workspace.onDidCloseTextDocument((document) => diagnostics.delete(document.uri)),
    vscode.workspace.onDidChangeConfiguration((event) => {
      if (event.affectsConfiguration("badLanguage")) {
        refreshAllDiagnostics();
        decorationProvider.refresh();
        sidebarProvider.refresh();
      }
    })
  );

  refreshAllDiagnostics();
}

function deactivate() {}

function runCurrentBadFile(mode) {
  const editor = vscode.window.activeTextEditor;
  if (!editor || !isBadDocument(editor.document) || isBadrcUri(editor.document.uri)) {
    vscode.window.showWarningMessage("Open a runnable .bad file first.");
    return;
  }

  const workspaceFolder = vscode.workspace.getWorkspaceFolder(editor.document.uri) || vscode.workspace.workspaceFolders?.[0];
  if (!workspaceFolder) {
    vscode.window.showErrorMessage("No workspace folder found.");
    return;
  }

  const filePath = editor.document.uri.fsPath;
  let relativePath = path.relative(workspaceFolder.uri.fsPath, filePath);
  if (!relativePath || relativePath.startsWith("..")) {
    relativePath = filePath;
  }
  const binaryPath = String(setting("runBinaryPath", "./bad") || "./bad").trim() || "./bad";
  const configPath = String(setting("defaultConfigPath", "examples/.badrc") || "examples/.badrc");
  const autoUseConfig = settingBool("runUseConfigByDefault", false);
  const includeConfig = mode === "config" || (mode === "auto" && autoUseConfig);

  const extraArgsValue = setting("runExtraArgs", []);
  const extraArgs = Array.isArray(extraArgsValue)
    ? extraArgsValue.map((item) => String(item)).filter((item) => item.trim().length > 0)
    : [];

  const parts = [
    formatShellArg(binaryPath),
    formatShellArg(relativePath)
  ];

  if (includeConfig) {
    parts.push("--config", formatShellArg(configPath));
  }

  for (const arg of extraArgs) {
    parts.push(formatShellArg(arg));
  }

  const terminalName = includeConfig ? "BAD Run (Config)" : "BAD Run";
  const terminal = vscode.window.createTerminal({
    name: terminalName,
    cwd: workspaceFolder.uri.fsPath
  });
  terminal.show(true);
  terminal.sendText(parts.join(" "), true);
}

function createCompletionProvider() {
  return {
    provideCompletionItems(document, position) {
      if (!settingBool("enableCompletions", true)) return [];

      const linePrefix = document.lineAt(position.line).text.slice(0, position.character);
      const items = [];

      for (const keyword of KEYWORDS) {
        const item = new vscode.CompletionItem(keyword, vscode.CompletionItemKind.Keyword);
        item.insertText = keyword;
        items.push(item);
      }

      for (const method of METHODS) {
        const item = new vscode.CompletionItem(method, vscode.CompletionItemKind.EnumMember);
        item.insertText = method;
        items.push(item);
      }

      if (/^\s*$/.test(linePrefix) || /^\s*[A-Za-z_][A-Za-z0-9_]*\s*=\s*$/.test(linePrefix)) {
        for (const option of TOP_LEVEL_OPTIONS) {
          const item = new vscode.CompletionItem(option, vscode.CompletionItemKind.Property);
          item.insertText = option;
          item.detail = "BAD top-level runtime option";
          items.push(item);
        }
      }

      if (/stats(\.[A-Za-z0-9_]*)?$/.test(linePrefix)) {
        for (const selector of STATS_SELECTORS) {
          const item = new vscode.CompletionItem(`stats.${selector}`, vscode.CompletionItemKind.Field);
          item.insertText = `stats.${selector}`;
          item.detail = "BAD runtime stats selector";
          items.push(item);
        }
      }

      if (/^\s*(expect|assert)\s+/.test(linePrefix)) {
        items.push(snippet("expect status", "status ${1:200}", "Assert response status"));
        items.push(snippet("expect status comparator", "status ${1|==,!=,<,<=,>,>=|} ${2:200}", "Assert status with comparator"));
        items.push(snippet("expect status in", "status in [${1:200}, ${2:201}]", "Assert status in list"));
        items.push(snippet("expect json", "json.${1:path.to.field} == \"${2:value}\"", "Assert JSON field"));
        items.push(snippet("expect exists", "json.${1:path.to.field} exists", "Assert JSON path exists"));
      }

      if (/^\s*send\s+$/.test(linePrefix)) {
        for (const method of METHODS) {
          const item = new vscode.CompletionItem(`send ${method}`, vscode.CompletionItemKind.Snippet);
          item.insertText = new vscode.SnippetString(method + ' "${1:/path}"');
          item.detail = `Send ${method} request`;
          items.push(item);
        }
        items.push(snippet("send req", "req ${1:templateName}", "Send request template"));
        items.push(snippet("send template", "template ${1:templateName}", "Send template alias"));
      }

      if (/^\s*method\s+$/.test(linePrefix)) {
        for (const method of METHODS) {
          const item = new vscode.CompletionItem(method, vscode.CompletionItemKind.EnumMember);
          item.insertText = method;
          item.detail = "HTTP method for request/template";
          items.push(item);
        }
      }

      if (/^\s*path\s+$/.test(linePrefix)) {
        items.push(snippet("path literal", '"${1:/path}"', "Template/request path literal"));
        items.push(snippet("path variable", "${1:pathVar}", "Template/request path variable"));
      }

      if (/^\s*let\s+[A-Za-z_][A-Za-z0-9_]*\s*=\s*$/.test(linePrefix)) {
        items.push(snippet("let json", "json.${1:path.to.field}", "Read from response JSON"));
        items.push(snippet("let status", "status", "Read last status"));
        items.push(snippet("let time_ms", "time_ms", "Read last response time"));
        items.push(snippet("let timer", "time ${1:flow_name}", "Read named timer"));
        items.push(snippet("let now_ms", "now_ms", "Read current epoch ms"));
        items.push(snippet("let env", "env ${1:API_KEY}", "Read environment variable"));
        items.push(snippet("let args", "args ${1:0}", "Read CLI arg"));
        items.push(snippet("let stats", "stats.${1:requests.total}", "Read runtime stats"));
      }

      if (/^\s*(print|fail_if|skip_if)\s+/.test(linePrefix)) {
        for (const selector of STATS_SELECTORS) {
          const item = new vscode.CompletionItem(`stats.${selector}`, vscode.CompletionItemKind.Field);
          item.insertText = `stats.${selector}`;
          item.detail = "Runtime stats selector";
          items.push(item);
        }
      }

      return items;
    }
  };
}

function createHoverProvider() {
  return {
    provideHover(document, position) {
      if (!settingBool("enableHovers", true)) return null;

      const range = document.getWordRangeAtPosition(position, /[A-Za-z_][A-Za-z0-9_.]*/);
      if (!range) return null;

      const token = document.getText(range);
      const statsEntry = resolveStatsHover(token);
      const entry = statsEntry || HOVER_DOCS[token];
      if (!entry) return null;

      const markdown = new vscode.MarkdownString(undefined, true);
      markdown.isTrusted = false;

      markdown.appendMarkdown(`**${entry.title || token}**\n\n`);
      markdown.appendMarkdown(`${entry.summary}\n\n`);

      if (entry.syntax) {
        markdown.appendMarkdown("**Syntax**\n");
        markdown.appendCodeblock(entry.syntax, "bad");
      }

      if (settingBool("hoverShowExamples", true) && entry.example) {
        markdown.appendMarkdown("\n**Example**\n");
        markdown.appendCodeblock(entry.example, "bad");
      }

      if (settingBool("hoverShowRelated", true) && Array.isArray(entry.related) && entry.related.length > 0) {
        const max = Math.max(1, Math.floor(settingNum("hoverMaxRelated", 5)));
        const related = entry.related.slice(0, max).map((item) => `\`${item}\``).join(", ");
        markdown.appendMarkdown(`\n**Related**: ${related}`);
      }

      return new vscode.Hover(markdown, range);
    }
  };
}

function resolveStatsHover(token) {
  if (token === "stats") {
    return {
      title: "stats",
      summary: "Built-in runtime statistics namespace.",
      syntax: "print stats\nprint stats.requests.total",
      example: 'fail_if stats.runtime.soft_errors > 0 because "runtime issue"',
      related: ["print", "fail_if", "expect", "time_ms"]
    };
  }

  if (!token.startsWith("stats.")) return null;
  const selector = token.slice("stats.".length);
  if (!STATS_SELECTORS.includes(selector)) return null;

  const human = selector.replace(/\./g, " -> ").replace(/_/g, " ");
  return {
    title: token,
    summary: `Runtime statistics selector for ${human}.`,
    syntax: `print ${token}`,
    example: `fail_if ${token} > ${selector.includes("time") ? 1000 : 0} because \"stats guard\"`,
    related: ["stats", "print", "fail_if", "skip_if"]
  };
}

function createSignatureProvider() {
  return {
    provideSignatureHelp(document, position) {
      if (!settingBool("enableSignatures", true)) return null;

      const line = document.lineAt(position.line).text.slice(0, position.character).trim();
      const help = new vscode.SignatureHelp();

      if (/^send\s+/.test(line)) {
        const direct = new vscode.SignatureInformation("send METHOD \"/path\" { body { ... } header { ... } }");
        direct.documentation = "Send direct HTTP request with optional inline block.";

        const req = new vscode.SignatureInformation("send req templateName with { path \"...\" body { ... } headers { ... } }");
        req.documentation = "Execute request template with optional inline overrides.";

        const tpl = new vscode.SignatureInformation("send template templateName with { ... }");
        tpl.documentation = "Alias form of send req.";

        help.signatures = [direct, req, tpl];
        help.activeSignature = /^send\s+(req|request)\s+/.test(line) ? 1 : (/^send\s+template\s+/.test(line) ? 2 : 0);
        help.activeParameter = 0;
        return help;
      }

      if (/^(expect|assert)\s+/.test(line)) {
        const statusSig = new vscode.SignatureInformation("expect status 200 | expect status >= 200 | expect status in [200,201]");
        statusSig.documentation = "Status assertion forms.";

        const jsonSig = new vscode.SignatureInformation("expect json.path == value | expect json.path exists | expect json.path contains \"x\"");
        jsonSig.documentation = "JSON assertion forms.";

        const timeSig = new vscode.SignatureInformation("expect time_ms < 500");
        timeSig.documentation = "Response timing assertion.";

        help.signatures = [statusSig, jsonSig, timeSig];
        help.activeSignature = /^\s*(expect|assert)\s+json\./.test(line) ? 1 : (/\btime_ms\b/.test(line) ? 2 : 0);
        help.activeParameter = 0;
        return help;
      }

      if (/^(skip_if|fail_if)\s+/.test(line)) {
        const guard = new vscode.SignatureInformation("fail_if left OP right because \"reason\" | skip_if left OP right because \"reason\"");
        guard.documentation = "Conditional guard statements.";
        help.signatures = [guard];
        help.activeSignature = 0;
        help.activeParameter = 0;
        return help;
      }

      return null;
    }
  };
}

function createSymbolProvider() {
  return {
    provideDocumentSymbols(document) {
      if (!settingBool("enableSymbols", true)) return [];

      const symbols = [];
      const lines = document.getText().split(/\r?\n/);

      for (let lineIndex = 0; lineIndex < lines.length; lineIndex++) {
        const line = lines[lineIndex];
        const trimmed = line.trim();
        if (!trimmed) continue;

        const testMatch = trimmed.match(/^(?:skip\s+|only\s+)?test\s+"([^"]+)"/);
        if (testMatch) {
          symbols.push(sym(testMatch[1], vscode.SymbolKind.Method, lineIndex, line.length));
          continue;
        }

        const groupMatch = trimmed.match(/^(?:skip\s+|only\s+)?group\s+"([^"]+)"/);
        if (groupMatch) {
          symbols.push(sym(groupMatch[1], vscode.SymbolKind.Namespace, lineIndex, line.length));
          continue;
        }

        const hookMatch = trimmed.match(/^(before_all|before_each|after_each|after_all|on_error|on_assertion_error|on_network_error|before_url|after_url|on_url_error)\b/);
        if (hookMatch) {
          symbols.push(sym(hookMatch[1], vscode.SymbolKind.Event, lineIndex, line.length));
          continue;
        }

        const reqMatch = trimmed.match(/^(?:export\s+)?(?:request|req|template)\s+([A-Za-z_][A-Za-z0-9_]*)/);
        if (reqMatch) {
          symbols.push(sym(reqMatch[1], vscode.SymbolKind.Function, lineIndex, line.length));
          continue;
        }

        const letMatch = trimmed.match(/^(?:export\s+)?let\s+([A-Za-z_][A-Za-z0-9_]*)\s*=/);
        if (letMatch) {
          symbols.push(sym(letMatch[1], vscode.SymbolKind.Variable, lineIndex, line.length));
          continue;
        }
      }

      return symbols;
    }
  };
}

function createCodeLensProvider() {
  return {
    provideCodeLenses(document) {
      if (!settingBool("enableCodeLens", true)) return [];

      const lenses = [];
      const lines = document.getText().split(/\r?\n/);

      for (let lineIndex = 0; lineIndex < lines.length; lineIndex++) {
        const line = lines[lineIndex];
        if (!/^\s*(?:skip\s+|only\s+)?(?:test|group)\s+"[^"]+"/.test(line)) continue;

        const range = new vscode.Range(lineIndex, 0, lineIndex, Math.max(1, line.length));
        lenses.push(new vscode.CodeLens(range, {
          title: "Run BAD File",
          command: "badLanguage.runCurrentFileAuto"
        }));
        lenses.push(new vscode.CodeLens(range, {
          title: "Run With Config",
          command: "badLanguage.runCurrentFileWithConfig"
        }));
      }

      return lenses;
    }
  };
}

function createImportLinkProvider() {
  return {
    async provideDocumentLinks(document) {
      if (!settingBool("enableImportLinks", true)) return [];

      const links = [];
      const lines = document.getText().split(/\r?\n/);

      for (let lineIndex = 0; lineIndex < lines.length; lineIndex++) {
        const line = lines[lineIndex];
        const match = line.match(/^\s*(import|use)\s+"([^"]+)"/);
        if (!match) continue;

        const rawPath = match[2];
        const quoted = `"${rawPath}"`;
        const start = line.indexOf(quoted);
        if (start < 0) continue;

        const range = new vscode.Range(lineIndex, start + 1, lineIndex, start + 1 + rawPath.length);
        const target = resolveImportTarget(document.uri, rawPath);
        if (!target) continue;

        try {
          await vscode.workspace.fs.stat(target);
          links.push(new vscode.DocumentLink(range, target));
        } catch {
          // ignore unresolved links
        }
      }

      return links;
    }
  };
}

function resolveImportTarget(sourceUri, rawPath) {
  if (!rawPath || typeof rawPath !== "string") return null;

  if (rawPath.startsWith("file://")) {
    try {
      return vscode.Uri.parse(rawPath);
    } catch {
      return null;
    }
  }

  if (/^[A-Za-z]:[\\/]/.test(rawPath) || rawPath.startsWith("\\\\")) {
    return vscode.Uri.file(rawPath);
  }

  if (rawPath.startsWith("/")) {
    const workspaceFolder = vscode.workspace.getWorkspaceFolder(sourceUri) || vscode.workspace.workspaceFolders?.[0];
    if (workspaceFolder) {
      return vscode.Uri.joinPath(workspaceFolder.uri, rawPath.replace(/^\/+/, ""));
    }
    return vscode.Uri.file(rawPath);
  }

  const sourceDir = sourceUri.with({ path: sourceUri.path.replace(/\/[^/]*$/, "") });
  return vscode.Uri.joinPath(sourceDir, rawPath);
}

function createSemanticProvider(legend) {
  return {
    provideDocumentSemanticTokens(document) {
      const builder = new vscode.SemanticTokensBuilder(legend);
      if (!settingBool("enableSemanticTokens", true)) {
        return builder.build();
      }

      const lines = document.getText().split(/\r?\n/);
      const keywordPattern = /\b(test|send|expect|assert|let|print|import|use|export|request|req|template|method|path|group|before_all|before_each|after_each|after_all|on_error|on_assertion_error|on_network_error|before_url|after_url|on_url_error|skip|skip_if|fail_if|only|because|with|body_merge|merge_body|if|and|or|not|else|else_if|contains|starts_with|ends_with|regex|in|retry|retry_count|retry_delay_ms|retry_backoff|retry_jitter_ms|stop|stop_all|sleep|body|payload|header|headers|status|json|exists|base|base_url|wait|timeout|save_history|save_steps|print_request|print_response|show_time|show_timestamp|json_view|json_pretty|flat|table|fail_fast|strict_runtime_errors|strict_runtime|remember_token|use_color|history_dir|history_file|history_format|history_mode|history_methods|history_exclude_methods|history_only_failed|history_include_headers|history_include_request_body|history_include_response_body|history_max_body_bytes|save_mode|save_methods|save_exclude_methods|save_only_failed|save_headers|save_request_body|save_response_body|save_max_body_bytes|log_level|only_req|only_import|bearer|env|args|time_start|time_stop|time|time_ms|now_ms|stats|true|false|null)\b/g;
      const operatorPattern = /(==|!=|<=|>=|=|<|>|:)/g;
      const numberPattern = /\b\d+(?:\.\d+)?\b/g;
      const stringPattern = /"([^"\\]|\\.)*"/g;

      for (let lineIndex = 0; lineIndex < lines.length; lineIndex++) {
        const content = lines[lineIndex];
        pushMatches(builder, lineIndex, content, keywordPattern, "keyword");
        pushMatches(builder, lineIndex, content, operatorPattern, "operator");
        pushMatches(builder, lineIndex, content, numberPattern, "number");
        pushMatches(builder, lineIndex, content, stringPattern, "string");
      }

      return builder.build();
    }
  };
}

function createDecorationProvider() {
  const emitter = new vscode.EventEmitter();
  return {
    onDidChangeFileDecorations: emitter.event,
    refresh() {
      emitter.fire(undefined);
    },
    provideFileDecoration(uri) {
      if (!settingBool("showFileDecorations", true)) return;
      if (uri.scheme !== "file") return;
      if (uri.path.endsWith(".bad") || isBadrcUri(uri)) {
        return {
          badge: "B",
          tooltip: "BAD DSL file",
          color: new vscode.ThemeColor("charts.green")
        };
      }
    }
  };
}

function createFormattingProvider() {
  const splitLogicalCondition = (conditionText) => {
    const out = [];
    const parts = conditionText.split(/\s+(and|or)\s+/);
    if (parts.length === 0) return [conditionText];

    out.push(parts[0].trim());
    for (let i = 1; i < parts.length; i += 2) {
      const op = parts[i];
      const rhs = parts[i + 1] || "";
      out.push(`${op} ${rhs}`.trim());
    }

    return out.filter(Boolean);
  };

  const maybeWrapLogicalLine = (trimmed, depth, indentUnit, wrapColumn) => {
    if (!settingBool("formatterWrapLogicalConditions", true)) return null;

    const ind = indentUnit.repeat(depth);
    const ifMatch = trimmed.match(/^if\s+(.+)\s*\{\s*$/);
    if (ifMatch && trimmed.length > wrapColumn && /\b(and|or)\b/.test(ifMatch[1])) {
      const wrapped = splitLogicalCondition(ifMatch[1]);
      if (wrapped.length > 1) {
        const lines = [`${ind}if ${wrapped[0]}`];
        for (let i = 1; i < wrapped.length; i++) {
          lines.push(`${ind}${indentUnit}${wrapped[i]}`);
        }
        lines[lines.length - 1] = `${lines[lines.length - 1]} {`;
        return lines;
      }
    }

    const guardMatch = trimmed.match(/^(skip_if|fail_if)\s+(.+)$/);
    if (guardMatch && trimmed.length > wrapColumn && /\b(and|or)\b/.test(guardMatch[2])) {
      const keyword = guardMatch[1];
      const after = guardMatch[2];
      const becauseIndex = after.indexOf(" because ");
      const condPart = becauseIndex >= 0 ? after.slice(0, becauseIndex) : after;
      const suffix = becauseIndex >= 0 ? after.slice(becauseIndex) : "";

      const wrapped = splitLogicalCondition(condPart);
      if (wrapped.length > 1) {
        const lines = [`${ind}${keyword} ${wrapped[0]}`];
        for (let i = 1; i < wrapped.length; i++) {
          lines.push(`${ind}${indentUnit}${wrapped[i]}`);
        }
        if (suffix) {
          lines[lines.length - 1] = `${lines[lines.length - 1]}${suffix}`;
        }
        return lines;
      }
    }

    return null;
  };

  return {
    provideDocumentFormattingEdits(document) {
      if (!settingBool("enableFormatting", true)) return [];

      const indentSize = Math.min(8, Math.max(1, Math.floor(settingNum("formatterIndentSize", 2))));
      const indentUnit = " ".repeat(indentSize);
      const wrapColumn = Math.min(220, Math.max(40, Math.floor(settingNum("formatterWrapColumn", 100))));

      const lines = document.getText().split(/\r?\n/);
      let depth = 0;
      const out = [];

      for (const rawLine of lines) {
        const trimmed = rawLine.trim();
        if (!trimmed) {
          out.push("");
          continue;
        }

        if (trimmed.startsWith("}")) {
          depth = Math.max(0, depth - 1);
        }

        const wrapped = maybeWrapLogicalLine(trimmed, depth, indentUnit, wrapColumn);
        if (wrapped && wrapped.length > 0) {
          out.push(...wrapped);
        } else {
          out.push(`${indentUnit.repeat(depth)}${trimmed}`);
        }

        const opens = (trimmed.match(/\{/g) || []).length;
        const closes = (trimmed.match(/\}/g) || []).length;
        depth = Math.max(0, depth + opens - closes);
      }

      const fullRange = new vscode.Range(
        document.positionAt(0),
        document.positionAt(document.getText().length)
      );
      return [vscode.TextEdit.replace(fullRange, out.join("\n"))];
    }
  };
}

function collectDiagnostics(document) {
  const diagnostics = [];
  if (isBadrcUri(document.uri)) {
    return diagnostics;
  }

  const text = document.getText();
  const lines = text.split(/\r?\n/);

  const topLevelHeuristics = settingBool("diagnosticsTopLevelHeuristics", true);
  const warnAmbiguousNot = settingBool("diagnosticsWarnNotAmbiguousNot", true);

  const braceDepthBeforeLine = [];
  let depth = 0;
  for (let lineIndex = 0; lineIndex < lines.length; lineIndex++) {
    braceDepthBeforeLine[lineIndex] = depth;
    const raw = lines[lineIndex];
    for (let charIndex = 0; charIndex < raw.length; charIndex++) {
      if (raw[charIndex] === "{") depth++;
      else if (raw[charIndex] === "}") depth = Math.max(0, depth - 1);
    }
  }

  const stack = [];
  for (let index = 0; index < text.length; index++) {
    if (text[index] === "{") stack.push(index);
    if (text[index] === "}") {
      if (stack.length === 0) {
        diagnostics.push(new vscode.Diagnostic(
          indexRange(document, index, index + 1),
          "Unmatched closing brace.",
          vscode.DiagnosticSeverity.Error
        ));
      } else {
        stack.pop();
      }
    }
  }

  for (const index of stack) {
    diagnostics.push(new vscode.Diagnostic(
      indexRange(document, index, index + 1),
      "Unmatched opening brace.",
      vscode.DiagnosticSeverity.Error
    ));
  }

  let quoteCount = 0;
  for (let index = 0; index < text.length; index++) {
    if (text[index] === '"' && text[index - 1] !== "\\") quoteCount++;
  }

  if (quoteCount % 2 !== 0) {
    const lastQuote = text.lastIndexOf('"');
    diagnostics.push(new vscode.Diagnostic(
      indexRange(document, Math.max(0, lastQuote), Math.max(1, lastQuote + 1)),
      "Possible unmatched double quote.",
      vscode.DiagnosticSeverity.Warning
    ));
  }

  const topLevelStatementRegex = /^(?:(?:skip|only)\s+)?(?:test|group)\b|^(?:import|use|let|if|fail_if|skip_if|sleep|print|stop|stop_all|time_start|time_stop|export|request|req|template|base|base_url|wait|timeout|retry|retry_delay_ms|retry_backoff|retry_jitter_ms|before_all|before_each|after_each|after_all|on_error|on_assertion_error|on_network_error|before_url|after_url|on_url_error|skip|only)\b/;
  const topLevelCommentOrCloserRegex = /^(#|\/\/|\/\*|\*|\})/;
  const topLevelOptionAssignmentRegex = /^[A-Za-z_][A-Za-z0-9_-]*\s*=\s*.+$/;

  const testOrGroupNameRegex = /^(?:(?:skip|only)\s+)?(?:test|group)\s+"(?:[^"\\]|\\.)+"\s*(?:because\s+"(?:[^"\\]|\\.)+")?\s*\{?\s*(#|\/\/.*)?$/;
  const sendMethodRegex = /^send\s+(GET|POST|PUT|PATCH|DELETE)\s+("[^"]+"|[A-Za-z_][A-Za-z0-9_./:-]*)(\s*\{)?\s*(#|\/\/.*)?$/;
  const sendReqRegex = /^send\s+(req|request|template)\s+[A-Za-z_][A-Za-z0-9_-]*(\s+with(?:\s*\{)?)?\s*(#|\/\/.*)?$/;
  const statusExpectRegex = /^(expect|assert)\s+status\s+(\d{3}|(==|!=|<=|>=|<|>)\s+.+|in\s*\[[^\]]+\])(\s*(#|\/\/).*)?$/;

  for (let lineIndex = 0; lineIndex < lines.length; lineIndex++) {
    const raw = lines[lineIndex];
    const line = raw.trim();
    if (!line) continue;

    const isTopLevel = braceDepthBeforeLine[lineIndex] === 0;

    if (
      topLevelHeuristics &&
      isTopLevel &&
      !topLevelStatementRegex.test(line) &&
      !topLevelCommentOrCloserRegex.test(line) &&
      !topLevelOptionAssignmentRegex.test(line)
    ) {
      diagnostics.push(new vscode.Diagnostic(
        new vscode.Range(lineIndex, 0, lineIndex, raw.length),
        "Unknown or misplaced BAD statement.",
        vscode.DiagnosticSeverity.Warning
      ));
    }

    if (isTopLevel && /^(?:(?:skip|only)\s+)?(?:test|group)\b/.test(line) && !testOrGroupNameRegex.test(line)) {
      diagnostics.push(new vscode.Diagnostic(
        new vscode.Range(lineIndex, 0, lineIndex, raw.length),
        "Expected quoted name after test/group.",
        vscode.DiagnosticSeverity.Error
      ));
    }

    if (/^send\b/.test(line) && !(sendMethodRegex.test(line) || sendReqRegex.test(line))) {
      diagnostics.push(new vscode.Diagnostic(
        new vscode.Range(lineIndex, 0, lineIndex, raw.length),
        "Invalid send syntax. Use: send METHOD \"/path\" { ... } or send req/request/template name.",
        vscode.DiagnosticSeverity.Error
      ));
    }

    if (/^(expect|assert)\s+status\b/.test(line) && !statusExpectRegex.test(line)) {
      diagnostics.push(new vscode.Diagnostic(
        new vscode.Range(lineIndex, 0, lineIndex, raw.length),
        "Status assertion looks malformed. Use: expect status 200, expect status >= 200, or expect status in [200,201].",
        vscode.DiagnosticSeverity.Warning
      ));
    }

    if (warnAmbiguousNot && /\bnot\s+[A-Za-z_][A-Za-z0-9_.]*\s*(==|!=|<=|>=|<|>)\s+/.test(line)) {
      diagnostics.push(new vscode.Diagnostic(
        new vscode.Range(lineIndex, 0, lineIndex, raw.length),
        "Suspicious condition: `not a == b` is ambiguous. Prefer `not (a == b)` or invert the operator.",
        vscode.DiagnosticSeverity.Warning
      ));
    }
  }

  return diagnostics;
}

function openSettingsPanel(context) {
  const panel = vscode.window.createWebviewPanel(
    "badSettings",
    "BAD Settings",
    vscode.ViewColumn.Active,
    { enableScripts: true }
  );

  panel.webview.html = getSettingsWebviewHtml(panel.webview);

  panel.webview.onDidReceiveMessage(async (message) => {
    if (message.type === "load") {
      panel.webview.postMessage({ type: "state", state: readSettingsState() });
      return;
    }

    if (message.type === "save") {
      await saveSettingsState(message.state || {});
      vscode.window.showInformationMessage("BAD settings saved.");
      panel.webview.postMessage({ type: "state", state: readSettingsState() });
      return;
    }

    if (message.type === "createBadrc") {
      await createExampleBadrc();
      return;
    }

    if (message.type === "useBadIcons") {
      await useBadIconTheme();
      return;
    }

    if (message.type === "openReadme") {
      await openExtensionReadme(context);
      return;
    }

    if (message.type === "openRuntimeConfig") {
      openRuntimeConfigPanel(context);
    }
  }, null, context.subscriptions);
}

function readSettingsState() {
  const state = {};
  for (const field of SETTINGS_FIELDS) {
    state[field.key] = setting(field.key, field.defaultValue);
  }
  return state;
}

async function saveSettingsState(state) {
  const configuration = cfg();
  for (const field of SETTINGS_FIELDS) {
    if (!(field.key in state)) continue;

    let value = state[field.key];
    if (field.type === "boolean") {
      value = !!value;
    } else if (field.type === "number") {
      const num = Number(value);
      if (!Number.isFinite(num)) continue;
      value = num;
    } else if (field.type === "array") {
      if (Array.isArray(value)) {
        value = value.map((item) => String(item));
      } else if (typeof value === "string") {
        try {
          const parsed = JSON.parse(value);
          value = Array.isArray(parsed) ? parsed.map((item) => String(item)) : [];
        } catch {
          value = [];
        }
      } else {
        value = [];
      }
    } else {
      value = String(value ?? "");
    }

    await configuration.update(field.key, value, vscode.ConfigurationTarget.Workspace);
  }
}

function resolveRuntimeConfigPath(rawPath) {
  const fallback = String(setting("defaultConfigPath", "examples/.badrc") || "examples/.badrc").trim() || "examples/.badrc";
  const normalizedPath = String(rawPath == null ? "" : rawPath).trim() || fallback;
  const workspaceFolder = vscode.workspace.workspaceFolders?.[0];

  let uri = null;
  if (/^[A-Za-z]:[\\/]/.test(normalizedPath) || normalizedPath.startsWith("/")) {
    uri = vscode.Uri.file(normalizedPath);
  } else if (workspaceFolder) {
    uri = vscode.Uri.joinPath(workspaceFolder.uri, normalizedPath);
  }

  return { path: normalizedPath, uri };
}

function parentDirectoryUri(fileUri) {
  const index = fileUri.path.lastIndexOf("/");
  const parentPath = index > 0 ? fileUri.path.slice(0, index) : "/";
  return fileUri.with({ path: parentPath });
}

async function loadRuntimeConfigState(pathInput) {
  const target = resolveRuntimeConfigPath(pathInput);
  let values = runtimeConfigDefaults();

  if (target.uri) {
    try {
      const bytes = await vscode.workspace.fs.readFile(target.uri);
      const text = Buffer.from(bytes).toString("utf8");
      const parsed = JSON.parse(text);
      if (parsed && typeof parsed === "object") {
        values = { ...values, ...parsed };
      }
    } catch {
      // Missing/invalid file falls back to defaults.
    }
  }

  return {
    path: target.path,
    values: normalizeRuntimeConfigValues(values)
  };
}

async function saveRuntimeConfigState(pathInput, valuesInput) {
  const target = resolveRuntimeConfigPath(pathInput);
  if (!target.uri) {
    throw new Error("No workspace folder found to resolve runtime config path.");
  }

  const normalizedValues = normalizeRuntimeConfigValues(valuesInput);
  const content = `${JSON.stringify(normalizedValues, null, 2)}\n`;

  await vscode.workspace.fs.createDirectory(parentDirectoryUri(target.uri));
  await vscode.workspace.fs.writeFile(target.uri, Buffer.from(content, "utf8"));

  return {
    path: target.path,
    uri: target.uri,
    values: normalizedValues
  };
}

function openRuntimeConfigPanel(context) {
  const panel = vscode.window.createWebviewPanel(
    "badRuntimeConfig",
    "BAD Runtime Config",
    vscode.ViewColumn.Active,
    { enableScripts: true }
  );

  panel.webview.html = getRuntimeConfigWebviewHtml(panel.webview);

  panel.webview.onDidReceiveMessage(async (message) => {
    try {
      if (message.type === "load") {
        const state = await loadRuntimeConfigState(message.path);
        panel.webview.postMessage({ type: "state", state });
        return;
      }

      if (message.type === "save") {
        const saved = await saveRuntimeConfigState(message.path, message.values || {});
        vscode.window.showInformationMessage(`Saved runtime config: ${saved.path}`);
        panel.webview.postMessage({ type: "state", state: { path: saved.path, values: saved.values } });
        return;
      }

      if (message.type === "open") {
        const target = resolveRuntimeConfigPath(message.path);
        if (!target.uri) {
          vscode.window.showErrorMessage("No workspace folder found to open runtime config.");
          return;
        }
        const doc = await vscode.workspace.openTextDocument(target.uri);
        await vscode.window.showTextDocument(doc, { preview: false });
      }
    } catch (error) {
      const messageText = error && error.message ? error.message : String(error);
      vscode.window.showErrorMessage(`BAD runtime config: ${messageText}`);
    }
  }, null, context.subscriptions);
}

async function createExampleBadrc() {
  const workspaceFolder = vscode.workspace.workspaceFolders?.[0];
  if (!workspaceFolder) {
    vscode.window.showErrorMessage("No workspace folder found.");
    return;
  }

  const examplesDir = vscode.Uri.joinPath(workspaceFolder.uri, "examples");
  const badrcUri = vscode.Uri.joinPath(examplesDir, ".badrc");

  const content = `{
  "base_url": "https://jsonplaceholder.typicode.com",
  "timeout": 10000,
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
  "print_response": true,
  "json_pretty": true,
  "json_view": false,
  "use_color": true,
  "fail_fast": false,
  "strict_runtime_errors": false,
  "log_level": "info"
}\n`;

  await vscode.workspace.fs.createDirectory(examplesDir);

  try {
    await vscode.workspace.fs.stat(badrcUri);
    const pick = await vscode.window.showQuickPick(["Open existing", "Overwrite"], {
      placeHolder: "examples/.badrc already exists"
    });
    if (!pick) return;

    if (pick === "Overwrite") {
      await vscode.workspace.fs.writeFile(badrcUri, Buffer.from(content, "utf8"));
      vscode.window.showInformationMessage("examples/.badrc overwritten.");
    }
  } catch {
    await vscode.workspace.fs.writeFile(badrcUri, Buffer.from(content, "utf8"));
    vscode.window.showInformationMessage("Created examples/.badrc.");
  }

  const doc = await vscode.workspace.openTextDocument(badrcUri);
  await vscode.window.showTextDocument(doc, { preview: false });
}

async function useBadIconTheme() {
  await vscode.workspace.getConfiguration("workbench").update("iconTheme", "bad-icons", vscode.ConfigurationTarget.Global);
  vscode.window.showInformationMessage("Switched icon theme to BAD File Icons.");
}

async function openExtensionReadme(context) {
  const readmeUri = vscode.Uri.joinPath(context.extensionUri, "README.md");
  const document = await vscode.workspace.openTextDocument(readmeUri);
  await vscode.window.showTextDocument(document, { preview: false });
}

function getSettingsWebviewHtml(webview) {
  const nonce = String(Date.now());
  const serializedFields = JSON.stringify(SETTINGS_FIELDS);

  return `<!doctype html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src ${webview.cspSource} 'unsafe-inline'; script-src 'nonce-${nonce}';" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>BAD Settings</title>
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

    :root {
      --accent: #10b981;
      --accent-dim: rgba(16, 185, 129, 0.12);
      --accent-border: rgba(16, 185, 129, 0.28);
      --surface: color-mix(in srgb, var(--vscode-editor-background) 92%, white 8%);
      --surface-raised: color-mix(in srgb, var(--vscode-editor-background) 80%, white 20%);
      --border: color-mix(in srgb, var(--vscode-panel-border) 60%, transparent 40%);
      --muted: color-mix(in srgb, var(--vscode-foreground) 55%, transparent 45%);
      --radius-sm: 8px;
      --radius-md: 12px;
      --radius-lg: 16px;
    }

    body {
      font-family: ui-sans-serif, -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      font-size: 13px;
      line-height: 1.5;
      color: var(--vscode-foreground);
      background: var(--vscode-editor-background);
      padding: 0;
      min-height: 100vh;
    }

    /* ── Top accent bar ── */
    .topbar {
      height: 3px;
      background: linear-gradient(90deg, #10b981, #3b82f6, #8b5cf6);
    }

    /* ── Header ── */
    .header {
      padding: 20px 24px 16px;
      border-bottom: 1px solid var(--border);
      display: flex;
      align-items: center;
      gap: 14px;
    }
    .header-icon {
      width: 38px;
      height: 38px;
      border-radius: var(--radius-sm);
      background: var(--accent-dim);
      border: 1px solid var(--accent-border);
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 18px;
      flex-shrink: 0;
    }
    .header-text h1 {
      font-size: 16px;
      font-weight: 700;
      letter-spacing: -0.2px;
      color: var(--vscode-foreground);
    }
    .header-text p {
      font-size: 12px;
      color: var(--muted);
      margin-top: 2px;
    }

    /* ── Main layout ── */
    .main {
      padding: 20px 24px 24px;
    }

    /* ── Grid ── */
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
      gap: 14px;
    }

    /* ── Section cards ── */
    .section {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: var(--radius-md);
      overflow: hidden;
      transition: border-color 0.15s;
    }
    .section:hover {
      border-color: var(--accent-border);
    }
    .section-header {
      padding: 10px 14px 8px;
      border-bottom: 1px solid var(--border);
      display: flex;
      align-items: center;
      gap: 7px;
    }
    .section-dot {
      width: 6px;
      height: 6px;
      border-radius: 50%;
      background: var(--accent);
      flex-shrink: 0;
    }
    .section h3 {
      font-size: 11px;
      font-weight: 700;
      letter-spacing: 0.6px;
      text-transform: uppercase;
      color: var(--muted);
    }
    .section-body {
      padding: 10px 14px 12px;
      display: flex;
      flex-direction: column;
      gap: 10px;
    }

    /* ── Fields ── */
    .field {
      display: flex;
      flex-direction: column;
      gap: 4px;
    }
    .field.inline {
      flex-direction: row;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
      padding: 5px 0;
      border-bottom: 1px solid color-mix(in srgb, var(--border) 50%, transparent 50%);
    }
    .field.inline:last-child {
      border-bottom: none;
      padding-bottom: 0;
    }
    .field label {
      font-size: 12.5px;
      font-weight: 600;
      color: var(--vscode-foreground);
    }

    .field input[type='text'],
    .field input[type='number'] {
      width: 100%;
      padding: 6px 9px;
      background: var(--vscode-input-background);
      border: 1px solid var(--vscode-input-border, var(--border));
      border-radius: var(--radius-sm);
      color: var(--vscode-input-foreground);
      font-size: 12.5px;
      font-family: inherit;
      transition: border-color 0.15s, box-shadow 0.15s;
      outline: none;
    }
    .field input[type='text']:focus,
    .field input[type='number']:focus {
      border-color: var(--accent);
      box-shadow: 0 0 0 2px var(--accent-dim);
    }

    /* Toggle switch */
    .toggle-wrap {
      position: relative;
      flex-shrink: 0;
    }
    .field input[type='checkbox'] {
      appearance: none;
      -webkit-appearance: none;
      width: 34px;
      height: 18px;
      border-radius: 9px;
      background: var(--vscode-input-border, #555);
      border: none;
      cursor: pointer;
      position: relative;
      transition: background 0.2s;
      display: block;
    }
    .field input[type='checkbox']::after {
      content: '';
      position: absolute;
      top: 2px;
      left: 2px;
      width: 14px;
      height: 14px;
      border-radius: 50%;
      background: #fff;
      transition: transform 0.2s;
    }
    .field input[type='checkbox']:checked {
      background: var(--accent);
    }
    .field input[type='checkbox']:checked::after {
      transform: translateX(16px);
    }

    /* ── Footer / Actions ── */
    .footer {
      padding: 16px 24px;
      border-top: 1px solid var(--border);
      display: flex;
      align-items: center;
      gap: 8px;
      flex-wrap: wrap;
    }
    .footer-tip {
      margin-left: auto;
      font-size: 11.5px;
      color: var(--muted);
    }
    .footer-tip code {
      font-size: 11px;
      padding: 1px 5px;
      border-radius: 4px;
      background: var(--accent-dim);
      color: var(--accent);
      font-family: ui-monospace, "Cascadia Code", Menlo, monospace;
    }

    button {
      display: inline-flex;
      align-items: center;
      gap: 5px;
      padding: 6px 13px;
      border-radius: var(--radius-sm);
      font-size: 12.5px;
      font-weight: 600;
      font-family: inherit;
      cursor: pointer;
      transition: opacity 0.15s, transform 0.1s;
      border: 1px solid transparent;
      line-height: 1.4;
    }
    button:active { transform: scale(0.97); }

    button#save {
      background: var(--accent);
      color: #fff;
      border-color: transparent;
    }
    button#save:hover { opacity: 0.88; }

    button.secondary {
      background: var(--surface-raised);
      color: var(--vscode-foreground);
      border-color: var(--border);
    }
    button.secondary:hover { border-color: var(--accent-border); }

    /* ── Toast ── */
    .toast {
      position: fixed;
      bottom: 18px;
      right: 18px;
      padding: 9px 16px;
      background: var(--accent);
      color: #fff;
      border-radius: var(--radius-sm);
      font-size: 12.5px;
      font-weight: 600;
      opacity: 0;
      transform: translateY(6px);
      transition: opacity 0.2s, transform 0.2s;
      pointer-events: none;
    }
    .toast.show { opacity: 1; transform: translateY(0); }
  </style>
</head>
<body>
  <div class="topbar"></div>

  <div class="header">
    <div class="header-icon">⚙</div>
    <div class="header-text">
      <h1>BAD Settings</h1>
      <p>Configure run commands, diagnostics, formatting, sidebar, hover docs, and providers</p>
    </div>
  </div>

  <div class="main">
    <div id="settings-grid" class="grid"></div>
  </div>

  <div class="footer">
    <button id="save">✓&nbsp; Save</button>
    <button id="reload" class="secondary">↺&nbsp; Reload</button>
    <button id="runtimecfg" class="secondary">Runtime Config</button>
    <button id="mkbadrc" class="secondary">Create .badrc</button>
    <button id="icons" class="secondary">BAD Icons</button>
    <button id="readme" class="secondary">README</button>
    <span class="footer-tip">Tip: search <code>badLanguage.</code> in Settings UI</span>
  </div>

  <div class="toast" id="toast">Settings saved</div>

  <script nonce="${nonce}">
    const vscode = acquireVsCodeApi();
    const fields = ${serializedFields};

    function renderFields() {
      const container = document.getElementById('settings-grid');
      container.innerHTML = '';

      const bySection = new Map();
      for (const field of fields) {
        if (!bySection.has(field.section)) bySection.set(field.section, []);
        bySection.get(field.section).push(field);
      }

      for (const [section, sectionFields] of bySection.entries()) {
        const sectionEl = document.createElement('div');
        sectionEl.className = 'section';

        const headerEl = document.createElement('div');
        headerEl.className = 'section-header';
        const dot = document.createElement('div');
        dot.className = 'section-dot';
        const title = document.createElement('h3');
        title.textContent = section;
        headerEl.appendChild(dot);
        headerEl.appendChild(title);
        sectionEl.appendChild(headerEl);

        const bodyEl = document.createElement('div');
        bodyEl.className = 'section-body';

        for (const field of sectionFields) {
          const row = document.createElement('div');
          row.className = field.type === 'boolean' ? 'field inline' : 'field';

          const label = document.createElement('label');
          label.htmlFor = field.key;
          label.textContent = field.label;

          let input;
          if (field.type === 'boolean') {
            input = document.createElement('input');
            input.type = 'checkbox';
            input.id = field.key;
            row.appendChild(label);
            row.appendChild(input);
          } else {
            input = document.createElement('input');
            input.type = field.type === 'number' ? 'number' : 'text';
            input.id = field.key;
            row.appendChild(label);
            row.appendChild(input);
          }

          bodyEl.appendChild(row);
        }

        sectionEl.appendChild(bodyEl);
        container.appendChild(sectionEl);
      }
    }

    function readFormState() {
      const state = {};
      for (const field of fields) {
        const input = document.getElementById(field.key);
        if (!input) continue;

        if (field.type === 'boolean') {
          state[field.key] = !!input.checked;
        } else if (field.type === 'number') {
          state[field.key] = Number(input.value || field.defaultValue || 0);
        } else if (field.type === 'array') {
          try {
            const parsed = JSON.parse(input.value || '[]');
            state[field.key] = Array.isArray(parsed) ? parsed : [];
          } catch {
            state[field.key] = [];
          }
        } else {
          state[field.key] = input.value;
        }
      }
      return state;
    }

    function applyState(state) {
      for (const field of fields) {
        const input = document.getElementById(field.key);
        if (!input) continue;

        const value = state && Object.prototype.hasOwnProperty.call(state, field.key)
          ? state[field.key]
          : field.defaultValue;

        if (field.type === 'boolean') {
          input.checked = !!value;
        } else if (field.type === 'array') {
          input.value = JSON.stringify(Array.isArray(value) ? value : []);
        } else {
          input.value = value == null ? '' : String(value);
        }
      }
    }

    window.addEventListener('message', (event) => {
      const data = event.data;
      if (!data || data.type !== 'state') return;
      applyState(data.state || {});
    });

    document.getElementById('save').addEventListener('click', () => {
      vscode.postMessage({ type: 'save', state: readFormState() });
      const toast = document.getElementById('toast');
      toast.classList.add('show');
      setTimeout(() => toast.classList.remove('show'), 2000);
    });

    document.getElementById('reload').addEventListener('click', () => {
      vscode.postMessage({ type: 'load' });
    });

    document.getElementById('runtimecfg').addEventListener('click', () => {
      vscode.postMessage({ type: 'openRuntimeConfig' });
    });

    document.getElementById('mkbadrc').addEventListener('click', () => {
      vscode.postMessage({ type: 'createBadrc' });
    });

    document.getElementById('icons').addEventListener('click', () => {
      vscode.postMessage({ type: 'useBadIcons' });
    });

    document.getElementById('readme').addEventListener('click', () => {
      vscode.postMessage({ type: 'openReadme' });
    });

    renderFields();
    vscode.postMessage({ type: 'load' });
  </script>
</body>
</html>`;
}

function getRuntimeConfigWebviewHtml(webview) {
  const nonce = String(Date.now());
  const serializedFields = JSON.stringify(RUNTIME_CONFIG_FIELDS);

  return `<!doctype html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src ${webview.cspSource} 'unsafe-inline'; script-src 'nonce-${nonce}';" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>BAD Runtime Config</title>
  <style>
    *, *::before, *::after { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: ui-sans-serif, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      color: var(--vscode-foreground);
      background: var(--vscode-editor-background);
    }
    .top {
      padding: 16px 18px;
      border-bottom: 1px solid var(--vscode-panel-border);
      display: flex;
      flex-direction: column;
      gap: 8px;
    }
    .title {
      font-size: 15px;
      font-weight: 700;
    }
    .subtitle {
      font-size: 12px;
      opacity: 0.8;
    }
    .path-row {
      display: grid;
      grid-template-columns: 110px 1fr;
      gap: 8px;
      align-items: center;
    }
    .path-row label {
      font-size: 12px;
      font-weight: 600;
    }
    .path-row input {
      width: 100%;
      padding: 6px 8px;
      border-radius: 6px;
      border: 1px solid var(--vscode-input-border, #555);
      background: var(--vscode-input-background);
      color: var(--vscode-input-foreground);
    }
    .main {
      padding: 14px 18px 18px;
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
      gap: 12px;
    }
    .section {
      border: 1px solid var(--vscode-panel-border);
      border-radius: 10px;
      overflow: hidden;
      background: color-mix(in srgb, var(--vscode-editor-background) 92%, white 8%);
    }
    .section h3 {
      margin: 0;
      padding: 9px 12px;
      font-size: 11px;
      text-transform: uppercase;
      letter-spacing: 0.5px;
      border-bottom: 1px solid var(--vscode-panel-border);
      opacity: 0.8;
    }
    .fields {
      padding: 10px 12px;
      display: flex;
      flex-direction: column;
      gap: 9px;
    }
    .field {
      display: flex;
      flex-direction: column;
      gap: 4px;
    }
    .field.inline {
      flex-direction: row;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
    }
    .field label {
      font-size: 12px;
      font-weight: 600;
    }
    .field input[type='text'],
    .field input[type='number'] {
      width: 100%;
      padding: 6px 8px;
      border-radius: 6px;
      border: 1px solid var(--vscode-input-border, #555);
      background: var(--vscode-input-background);
      color: var(--vscode-input-foreground);
      font-size: 12px;
    }
    .field input[type='checkbox'] {
      width: 16px;
      height: 16px;
    }
    .footer {
      position: sticky;
      bottom: 0;
      display: flex;
      gap: 8px;
      flex-wrap: wrap;
      padding: 12px 18px;
      border-top: 1px solid var(--vscode-panel-border);
      background: color-mix(in srgb, var(--vscode-editor-background) 95%, black 5%);
    }
    button {
      border: 1px solid transparent;
      border-radius: 7px;
      padding: 6px 12px;
      font-size: 12px;
      cursor: pointer;
      font-weight: 600;
    }
    #save { background: #10b981; color: #fff; }
    .secondary {
      background: color-mix(in srgb, var(--vscode-editor-background) 84%, white 16%);
      color: var(--vscode-foreground);
      border-color: var(--vscode-panel-border);
    }
    .hint {
      margin-left: auto;
      font-size: 11px;
      opacity: 0.7;
      align-self: center;
    }
  </style>
</head>
<body>
  <div class="top">
    <div class="title">BAD Runtime Config</div>
    <div class="subtitle">Edit one .badrc with advanced history controls, then run BAD with --config.</div>
    <div class="path-row">
      <label for="configPath">Config Path</label>
      <input id="configPath" type="text" value="examples/.badrc" />
    </div>
  </div>

  <div id="sections" class="main"></div>

  <div class="footer">
    <button id="save">Save .badrc</button>
    <button id="reload" class="secondary">Reload</button>
    <button id="open" class="secondary">Open File</button>
    <span class="hint">Modes: all | per-file | per-test | off</span>
  </div>

  <script nonce="${nonce}">
    const vscode = acquireVsCodeApi();
    const fields = ${serializedFields};

    function renderSections() {
      const container = document.getElementById('sections');
      container.innerHTML = '';

      const groups = new Map();
      for (const field of fields) {
        if (!groups.has(field.section)) groups.set(field.section, []);
        groups.get(field.section).push(field);
      }

      for (const [section, list] of groups.entries()) {
        const box = document.createElement('div');
        box.className = 'section';

        const title = document.createElement('h3');
        title.textContent = section;
        box.appendChild(title);

        const fieldsWrap = document.createElement('div');
        fieldsWrap.className = 'fields';

        for (const field of list) {
          const row = document.createElement('div');
          row.className = field.type === 'boolean' ? 'field inline' : 'field';

          const label = document.createElement('label');
          label.htmlFor = field.key;
          label.textContent = field.label;
          row.appendChild(label);

          const input = document.createElement('input');
          input.id = field.key;
          input.type = field.type === 'boolean' ? 'checkbox' : (field.type === 'number' ? 'number' : 'text');
          row.appendChild(input);

          fieldsWrap.appendChild(row);
        }

        box.appendChild(fieldsWrap);
        container.appendChild(box);
      }
    }

    function applyValues(values) {
      for (const field of fields) {
        const input = document.getElementById(field.key);
        if (!input) continue;
        const value = values && Object.prototype.hasOwnProperty.call(values, field.key)
          ? values[field.key]
          : field.defaultValue;

        if (field.type === 'boolean') input.checked = !!value;
        else input.value = value == null ? '' : String(value);
      }
    }

    function collectValues() {
      const out = {};
      for (const field of fields) {
        const input = document.getElementById(field.key);
        if (!input) continue;
        if (field.type === 'boolean') out[field.key] = !!input.checked;
        else if (field.type === 'number') out[field.key] = Number(input.value || field.defaultValue || 0);
        else out[field.key] = input.value;
      }
      return out;
    }

    function currentPath() {
      return document.getElementById('configPath').value || 'examples/.badrc';
    }

    window.addEventListener('message', (event) => {
      const data = event.data;
      if (!data || data.type !== 'state') return;
      document.getElementById('configPath').value = data.state && data.state.path ? data.state.path : currentPath();
      applyValues((data.state && data.state.values) || {});
    });

    document.getElementById('reload').addEventListener('click', () => {
      vscode.postMessage({ type: 'load', path: currentPath() });
    });

    document.getElementById('save').addEventListener('click', () => {
      vscode.postMessage({ type: 'save', path: currentPath(), values: collectValues() });
    });

    document.getElementById('open').addEventListener('click', () => {
      vscode.postMessage({ type: 'open', path: currentPath() });
    });

    renderSections();
    vscode.postMessage({ type: 'load', path: currentPath() });
  </script>
</body>
</html>`;
}

function snippet(label, value, detail) {
  const item = new vscode.CompletionItem(label, vscode.CompletionItemKind.Snippet);
  item.insertText = new vscode.SnippetString(value);
  item.detail = detail;
  return item;
}

function sym(name, kind, line, lineLength) {
  const range = new vscode.Range(line, 0, line, Math.max(1, lineLength));
  return new vscode.DocumentSymbol(name, "", kind, range, range);
}

function pushMatches(builder, line, content, regex, tokenType) {
  regex.lastIndex = 0;
  let match;
  while ((match = regex.exec(content)) !== null) {
    builder.push(line, match.index, match[0].length, typeIndex(tokenType), 0);
  }
}

function typeIndex(name) {
  return {
    keyword: 0,
    string: 1,
    number: 2,
    operator: 3,
    variable: 4
  }[name] ?? 0;
}

function indexRange(document, start, end) {
  const s = document.positionAt(start);
  const e = document.positionAt(end);
  return new vscode.Range(s, e);
}

module.exports = {
  activate,
  deactivate
};