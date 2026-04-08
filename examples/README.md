# BAD Examples Suite (Grouped)

Author: clixiya
Updated: 2026-04-05

The examples are now grouped by topic and renamed with descriptive file names.
This makes it easier to learn BAD step by step and easier to find a specific pattern quickly.

## Folder Structure

```text
examples/
  .badrc
  README.md
  01-basics/
  02-imports/
  03-hooks-and-flow/
  04-runtime/
```

## Naming Rules

- file names describe intent, not history
- names are action-oriented and searchable
- each file has one primary teaching objective
- every `.bad` file includes a top header with: File, Author, Purpose, Flow, Features

## 01-basics

- `quick_start_demo.bad`: first run, direct GET/POST, core assertions
- `auth_header_template.bad`: auth header variable + reusable request template
- `crud_operations.bad`: GET/POST/PUT/PATCH/DELETE request shapes

## 02-imports

- `shared_exports.bad`: shared exported paths and templates
- `reusable_templates.bad`: reusable exported template module
- `selective_source.bad`: source module for selective imports
- `selective_import_alias_demo.bad`: selective import with alias usage
- `template_alias_import_demo.bad`: template alias import pattern
- `only_import_only_request_demo.bad`: `only import` + `only req` focused flow
- `composed_import_suite.bad`: suite composed from multiple import sources
- `top_level_options_with_import.bad`: top-level runtime options with imports
- `compat_shared_template.bad`: compatibility shared module
- `compat_import_consumer.bad`: compatibility import consumer

## 03-hooks-and-flow

- `group_lifecycle_with_overrides.bad`: group lifecycle hooks + template override
- `after_hooks_suite_demo.bad`: before/after hook behavior
- `object_template_url_hooks.bad`: object reuse + URL hooks + template override
- `regression_object_template_hooks.bad`: deterministic regression for object/template/hook mix
- `skip_with_reason_controls.bad`: skip test/group with explicit reason
- `only_group_scope_filter.bad`: inline only-group scope behavior

## 04-runtime

- `advanced_runtime_controls.bad`: timers, conditions, retry controls
- `runtime_stats_report.bad`: runtime stats assertions and printing
- `benchmark_baseline.bad`: lightweight multi-endpoint timing baseline

## Suggested Learning Order

1. `examples/01-basics/quick_start_demo.bad`
2. `examples/01-basics/auth_header_template.bad`
3. `examples/02-imports/shared_exports.bad`
4. `examples/02-imports/composed_import_suite.bad`
5. `examples/03-hooks-and-flow/group_lifecycle_with_overrides.bad`
6. `examples/03-hooks-and-flow/object_template_url_hooks.bad`
7. `examples/04-runtime/advanced_runtime_controls.bad`
8. `examples/04-runtime/runtime_stats_report.bad`

## How To Run

Run one file:

```bash
./bad examples/01-basics/quick_start_demo.bad --config examples/.badrc
```

Run one import-focused suite:

```bash
./bad examples/02-imports/composed_import_suite.bad --config examples/.badrc
```

Run all files recursively in order:

```bash
find examples -type f -name '*.bad' | sort | while read -r f; do
  ./bad "$f" --config examples/.badrc --no-color || break
done
```

## Authoring Guidance

- keep tests deterministic (`/users/1`, `/todos/1`, `/posts/1`)
- keep import paths absolute from repo root (for example `examples/02-imports/shared_exports.bad`)
- when importing templates selectively, also import dependent exported vars
