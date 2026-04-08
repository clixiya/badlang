# Changelog

## 1.0.1
- Fixed false diagnostics on valid `skip test ... because "reason"` and `skip group ... because "reason" {` forms.
- Updated top-level test/group validation regex to accept optional `because` reason clauses.

## 1.0.0
- Production release for BAD Language Tools.
- Aligned editor syntax coverage with BAD runtime/parser vocabulary, including `method`, `path`, `stats`, and full top-level runtime option keywords.
- Improved semantic token coverage for request-template directives and runtime options (`retry_count`, `save_history`, `strict_runtime_errors`, `only_req`, `only_import`, etc.).
- Improved diagnostics to reduce false positives for valid `send req/request/template` forms and hyphenated identifiers/options.
- Extended hover/completion support for request-template directives and runtime filtering options.
- Packaged and validated as production-ready VSIX.

## 0.3.2
- Cleaned extension asset structure by moving branding and icon files under `assets/`.
- Expanded icon theme mappings beyond `.bad` to include `.badrc`, BAD run scripts, history logs, and key BAD project folders.
- Updated packaging instructions/scripts to produce VSIX output in `dist/`.

## 0.3.1
- Replaced generated snippet catalog with a fully hand-written, unique snippet collection.
- Removed snippet generator script and related package script wiring.

## 0.3.0
- Added BAD Activity Bar sidebar with quick actions and icon.
- Added extensive customization settings for run behavior, diagnostics, formatting, hover behavior, symbols, semantic tokens, code lens, import links, and sidebar visibility.
- Refactored extension core for cleaner feature toggles and better maintainability.
- Added intelligent hover docs with syntax/examples/related references and stats selector awareness.
- Added code lens actions above test/group declarations.
- Added document links for import/use file paths.
- Expanded folding markers to support hooks, conditionals, and template alias blocks.
- Expanded snippet catalog to 322 snippets.
- Improved run command pipeline to support configurable binary path and extra CLI args.

## 0.2.2
- Added new BAD keywords to editor features: `template`, `on_error`, `on_assertion_error`, `on_network_error`, `before_url`, `after_url`, `on_url_error`.
- Extended completions and hovers for template aliasing and error/URL hook blocks.
- Updated symbol and semantic token support to include `template` declarations.
- Updated diagnostics to accept new top-level hook statements and `send template ...` syntax.
- Added snippets for error hooks, URL hooks, and template override `body_merge` option.

## 0.2.1
- Fixed false diagnostics on valid nested statements inside test/group/request blocks.
- Improved `send` syntax validation for block forms and variable path forms.
- Added one-click `examples/.badrc` creation command and settings panel action.
- Added one-click `Use BAD File Icons` command and settings panel action.
- Improved diagnostics to avoid flagging valid top-level option assignments.

## 0.2.0
- Added `BAD: Open Settings Panel` webview UI.
- Added BAD file decorations so BAD files stand out without replacing your full icon theme.
- Added document formatter and toggle setting.
- Expanded diagnostics for common BAD syntax mistakes.
- Improved completion suggestions for imports/assertions/hooks/base URL.
- Added many new snippets including `badrc` and `examplecfg` for `examples/.badrc`.
- Added `.badrc` as a recognized BAD filename.
- Improved language configuration (indentation rules, block comments).
- Expanded grammar scopes (variables, JSON-like keys, block comments).

## 0.1.0
- Initial BAD language extension release.
- Added grammar, snippets, IntelliSense, diagnostics, commands, symbols, themes, and icon theme.
