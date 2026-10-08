#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT

"$ROOT/run.sh" "$ROOT/tests/fixtures/remove_source.n" -o "$BUILD/remove"
actual="$($BUILD/remove)"
expected='562'
if [[ "$actual" != "$expected" ]]; then
    printf 'remove test failed\nexpected:\n%s\nactual:\n%s\n' "$expected" "$actual" >&2
    exit 1
fi
printf 'remove test passed\n'
