#!/usr/bin/env bash

set -e
cd "$(dirname "$0")/.."

NEVO_RETURN_TMP="$(mktemp -d)"
cleanup() {
    rm -rf "$NEVO_RETURN_TMP"
}
trap cleanup EXIT

./run.sh tests/fixtures/return_source.n "$NEVO_RETURN_TMP/return-test"
"$NEVO_RETURN_TMP/return-test" > "$NEVO_RETURN_TMP/return-output.txt"
diff -u tests/fixtures/return_expected.txt "$NEVO_RETURN_TMP/return-output.txt"

clang -w -DNEVO_LIBRARY_BUILD -Iinclude \
    src/format_main.c src/format.c src/source_io.c src/source_graph.c \
    -o "$NEVO_RETURN_TMP/format"
if "$NEVO_RETURN_TMP/format" tests/fixtures/invalid_return_type.n "$NEVO_RETURN_TMP/invalid.json" 2> "$NEVO_RETURN_TMP/invalid.err"; then
    echo "expected invalid return type to fail" >&2
    exit 1
fi
grep -q "txt function '_bad' cannot return num" "$NEVO_RETURN_TMP/invalid.err"
echo "v1.2 function return tests passed"
