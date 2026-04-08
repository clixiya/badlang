#!/usr/bin/env bash
set -euo pipefail

# Quick command pack for common runs.
# Copy any line below and run.

# Build binary
make

# Pick the available binary name
BAD_BIN="./bad"
if [[ -x "./bad.exe" ]]; then
	BAD_BIN="./bad.exe"
fi

# Default demo with config
"$BAD_BIN" examples/01-basics/quick_start_demo.bad --config examples/.badrc

# Pretty JSON + history
"$BAD_BIN" examples/01-basics/quick_start_demo.bad --config examples/.badrc --json-pretty --save --save-steps --save-file .bad-history/all-runs.jsonl

# Focus by group/only syntax inside file
"$BAD_BIN" examples/03-hooks-and-flow/skip_with_reason_controls.bad

# Group hooks + request overrides
"$BAD_BIN" examples/03-hooks-and-flow/group_lifecycle_with_overrides.bad

# Run a custom file with ad-hoc flags
# "$BAD_BIN" examples/your_file.bad --config examples/.badrc --flat --print-request --fail-fast
