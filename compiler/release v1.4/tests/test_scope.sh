#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT
cd "$ROOT"

"$ROOT/run.sh" "$ROOT/tests/fixtures/global_scope_source.n" -o "$BUILD/scope"
actual="$($BUILD/scope)"
[[ "$actual" == $'542\nscore=3|S1' ]]

grep -q '_gv_score' "$BUILD/scope.asm"
if grep -q '_gv_x' "$BUILD/scope.asm"; then
    echo 'ordinary declarations unexpectedly used global storage' >&2
    exit 1
fi

printf 'local-by-default and explicit global scope tests passed\n'
