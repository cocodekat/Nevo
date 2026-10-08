#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT

"$ROOT/run.sh" "$ROOT/tests/fixtures/input_source.n" -o "$BUILD/input"
actual="$(printf 'Philip\n23\n' | "$BUILD/input")"
expected='name: age: helloPhilip23'
if [[ "$actual" != "$expected" ]]; then
    printf 'input test failed\nexpected:\n%s\nactual:\n%s\n' "$expected" "$actual" >&2
    exit 1
fi
printf 'input test passed\n'
