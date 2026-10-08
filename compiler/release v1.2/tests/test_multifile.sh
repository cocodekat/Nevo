#!/usr/bin/env bash

set -e
cd "$(dirname "$0")/.."

NEVO_MULTI_TMP="$(mktemp -d)"
cleanup() {
    rm -rf "$NEVO_MULTI_TMP"
}
trap cleanup EXIT

./run.sh \
    'tests/fixtures/file one.n' \
    'tests/fixtures/file two.n' \
    'tests/fixtures/file three.n' \
    -o "$NEVO_MULTI_TMP/multi out"

"$NEVO_MULTI_TMP/multi out" > "$NEVO_MULTI_TMP/output.txt"
diff -u tests/fixtures/multifile_expected.txt "$NEVO_MULTI_TMP/output.txt"
test "$(lipo -archs "$NEVO_MULTI_TMP/multi out")" = "arm64"
echo "v1.2 multi-file tests passed"
