#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT
"$ROOT/run.sh" "$ROOT/tests/fixtures/control_array_source.n" -o "$BUILD/features"
actual="$($BUILD/features)"
expected=$'ready\nnot false\n4\n2\nelse-if'
[[ "$actual" == "$expected" ]]
"$ROOT/run.sh" "$ROOT/tests/fixtures/input_range_source.n" -o "$BUILD/input-range"
range_actual="$(printf 'nope\n999\n23\n' | "$BUILD/input-range")"
[[ "$range_actual" == *'Invalid number. Try again.'* ]]
[[ "$range_actual" == *'Number out of range. Try again.'* ]]
[[ "$range_actual" == *'23' ]]
if "$ROOT/run.sh" "$ROOT/tests/fixtures/missing_semicolon.n" -o "$BUILD/bad" 2>"$BUILD/bad.err"; then
    echo 'missing semicolon unexpectedly compiled' >&2
    exit 1
fi
grep -q "expected ';' after statement" "$BUILD/bad.err"
printf 'bool, arrays, loops, sleep, and input validation tests passed\n'
