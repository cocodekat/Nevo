#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT

"$ROOT/run.sh" "$ROOT/tests/fixtures/print_exact_source.n" -o "$BUILD/print-exact"
actual="$($BUILD/print-exact)"
expected='Hello, Philip! Score: 7'
[[ "$actual" == "$expected" ]]
printf 'exact print test passed\n'
