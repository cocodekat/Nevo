#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT
cd "$BUILD"
"$ROOT/run.sh" "$ROOT/tests/fixtures/let_source.n" -o "$BUILD/let"
[[ "$("$BUILD/let")" == '3|nevo|9' ]]
printf 'let ... be declaration tests passed\n'
