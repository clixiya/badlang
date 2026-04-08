# BAD Language Tools (VS Code Extension)

Advanced VS Code support for BAD DSL (`.bad`, `.badrc`) with fast authoring workflows and deep customization.

Production status: `1.1.0` stable release.

## Highlights

- Curated hand-written snippets for requests, hooks, assertions, options, stats, and scenarios
- Context-aware IntelliSense + method + stats selector completions
- Intelligent hover docs with syntax, examples, and related keywords
- Signature help for `send`, `expect/assert`, and guard clauses
- Runtime-aligned syntax support including request-template directives (`method`, `path`) and top-level runtime options
- Document symbols for tests, groups, hooks, templates, and variables
- CodeLens actions on tests/groups for one-click run
- Import/use path links (click to open target BAD file)
- Configurable diagnostics and formatter behavior with reduced false positives for valid `send` forms
- BAD Activity Bar sidebar (icon + quick actions)
- Dedicated runtime config webview for editing one `.badrc` file (including advanced history controls)
- BAD icon theme for DSL files, `.badrc`, history logs, run scripts, and key BAD folders

## Install

### Build VSIX

```bash
cd extenstion
npm install
npm run package
```

### Install VSIX

1. Open Extensions panel
2. Select `Install from VSIX...`
3. Pick generated `.vsix`
4. Reload window

## Commands

- `BAD: Run` (`badLanguage.runCurrentFile`)
- `BAD: Run Config` (`badLanguage.runCurrentFileWithConfig`)
- `BAD: Run File (Auto)` (`badLanguage.runCurrentFileAuto`)
- `BAD: Settings` (`badLanguage.openSettingsPanel`)
- `BAD: Runtime Config` (`badLanguage.openRuntimeConfigPanel`)
- `BAD: New .badrc` (`badLanguage.createExampleBadrc`)
- `BAD: Use Icons` (`badLanguage.useBadIconTheme`)
- `BAD: Open README` (`badLanguage.openExtensionReadme`)
- `BAD: Refresh Sidebar` (`badLanguage.refreshSidebar`)

## Sidebar

The BAD icon appears in the Activity Bar and opens the **BAD Quick Actions** view.

Quick actions include:

- Run current BAD file
- Run with config
- Open settings panel
- Open runtime config panel
- Create `examples/.badrc`
- Enable BAD icons
- Open docs

## Intelligent Hover Behavior

Hover docs include:

- concise meaning
- syntax block
- example block
- related keyword list (configurable)

Hover support includes BAD keywords and `stats` / `stats.*` selectors.

## Snippet Coverage

Snippet packs include:

- core structures: tests, groups, hooks, templates
- method variants: GET/POST/PUT/PATCH/DELETE families
- assertion variants: comparators, existence, contains, regex, in-list
- status code snippets: broad HTTP code coverage
- runtime options snippets (`base`, `timeout`, `strict_runtime_errors`, etc.)
- stats selector snippets (`stats.requests.*`, `stats.assertions.*`, `stats.runtime.*`, `stats.timers.*`)
- scenario boilerplates
- `.badrc` JSON blocks

## Settings

Namespace: `badLanguage.*`

### Run

- `defaultConfigPath`
- `runBinaryPath`
- `runExtraArgs`
- `runUseConfigByDefault`

### Diagnostics

- `enableDiagnostics`
- `diagnosticsTopLevelHeuristics`
- `diagnosticsWarnNotAmbiguousNot`

### Formatting

- `enableFormatting`
- `formatterIndentSize`
- `formatterWrapColumn`
- `formatterWrapLogicalConditions`

### Editor Features

- `enableCompletions`
- `enableHovers`
- `hoverShowExamples`
- `hoverShowRelated`
- `hoverMaxRelated`
- `enableSignatures`
- `enableSymbols`
- `enableSemanticTokens`
- `enableCodeLens`
- `enableImportLinks`

### Visual + Sidebar

- `showFileDecorations`
- `sidebarShowRunActions`
- `sidebarShowDocs`

## Diagnostics Notes

Diagnostics are intentionally lightweight/editor-side heuristics. Runtime parser/executor remains source of truth.

## Packaging

```bash
cd extenstion
npm run package
```

## Quick Smoke Test

1. Open any `.bad` file
2. Confirm sidebar BAD icon and quick actions appear
3. Trigger snippet insertion and hover on keywords
4. Run `BAD: Run File (Auto)`
5. Open settings panel and toggle a few features
