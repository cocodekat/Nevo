#!/usr/bin/env bash

set -e
cd "$(dirname "$0")/.."

NEVO_INCLUDE_TMP="$(mktemp -d)"
cleanup() {
    rm -rf "$NEVO_INCLUDE_TMP"
}
trap cleanup EXIT

# Supplying both files must not compile the included file twice.
./run.sh tests/fixtures/include_main.n tests/fixtures/include_add.n \
    -o "$NEVO_INCLUDE_TMP/explicit-and-include"
"$NEVO_INCLUDE_TMP/explicit-and-include" > "$NEVO_INCLUDE_TMP/explicit.txt"
diff -u tests/fixtures/include_expected.txt "$NEVO_INCLUDE_TMP/explicit.txt"

# The include directive must also discover the dependency by itself.
./run.sh tests/fixtures/include_main.n -o "$NEVO_INCLUDE_TMP/include-only"
"$NEVO_INCLUDE_TMP/include-only" > "$NEVO_INCLUDE_TMP/include.txt"
diff -u tests/fixtures/include_expected.txt "$NEVO_INCLUDE_TMP/include.txt"

test "$(lipo -archs "$NEVO_INCLUDE_TMP/include-only")" = "arm64"
echo "v1.2 include and implicit-global tests passed"
