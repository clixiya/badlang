#!/usr/bin/env bash
set -euo pipefail

# Quick runner for bad DSL tests.
# Usage examples:
#   ./run_bad.sh
#   ./run_bad.sh examples/01-basics/quick_start_demo.bad
#   ./run_bad.sh examples/01-basics/quick_start_demo.bad --config examples/.badrc --json-pretty --save

FILE="${1:-examples/01-basics/quick_start_demo.bad}"
shift || true

BIN="./bad"
if [[ -x "./bad.exe" ]]; then
	BIN="./bad.exe"
fi

"$BIN" "$FILE" --config examples/.badrc "$@"
