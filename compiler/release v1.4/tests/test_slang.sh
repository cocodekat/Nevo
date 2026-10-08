#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT
cd "$ROOT"

./run.sh tests/fixtures/slang_source.n -o "$BUILD/slang"
actual="$(printf 'neo\n' | "$BUILD/slang" 2>"$BUILD/debug.txt")"
[[ "$actual" == neo\|sus\|nah\|* ]]
[[ "$actual" == *'|5|7|3|3|new|standalone|2|sidequest|9|done' ]]
grep -q 'bruh: reached source line' "$BUILD/debug.txt"

./run.sh tests/fixtures/slang_failure.n -o "$BUILD/failure"
if "$BUILD/failure" >"$BUILD/failure.out" 2>"$BUILD/failure.err"; then
    echo 'skillissue unexpectedly returned success' >&2
    exit 1
fi
grep -q 'skill issue: deliberate failure' "$BUILD/failure.err"

printf 'all slang feature tests passed\n'
