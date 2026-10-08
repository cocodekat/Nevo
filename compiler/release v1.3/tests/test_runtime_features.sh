#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT

"$ROOT/run.sh" "$ROOT/tests/fixtures/runtime_replace_source.n" -o "$BUILD/replace"
replace_actual="$($BUILD/replace | tr -d '\n')"
[[ "$replace_actual" == "5621" ]]

"$ROOT/run.sh" "$ROOT/tests/fixtures/logical_rand_source.n" -o "$BUILD/logical"
logical_actual="$($BUILD/logical)"
logical_expected='symbol-andsymbol-orword-andword-orrand-in-range'
[[ "$logical_actual" == "$logical_expected" ]]

printf 'runtime replacement, logical operators, and rand tests passed\n'
