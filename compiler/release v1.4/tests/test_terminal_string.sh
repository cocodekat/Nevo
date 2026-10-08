#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT
cd "$ROOT"

./run.sh tests/fixtures/terminal_string_source.n -o "$BUILD/features"
actual="$(printf 'K' | "$BUILD/features")"

[[ "$actual" == hello,world\|11\|1\|2\|hello\|world\|42\|wheel-ok\|* ]]
[[ "$actual" == *'|K|'* ]]
[[ "$actual" == *$'\033[2J\033[Hcleared' ]]

printf 'terminal, strings, wheel, if\x27nt, and maybe tests passed\n'
