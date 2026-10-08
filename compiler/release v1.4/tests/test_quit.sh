#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/.."

NEVO_QUIT_TMP="$(mktemp -d)"
cleanup() {
    rm -rf "$NEVO_QUIT_TMP"
}
trap cleanup EXIT

./run.sh tests/fixtures/quit_source.n "$NEVO_QUIT_TMP/quit-test"
"$NEVO_QUIT_TMP/quit-test" > "$NEVO_QUIT_TMP/quit-output.txt"
printf 'before quit' > "$NEVO_QUIT_TMP/quit-expected.txt"
diff -u "$NEVO_QUIT_TMP/quit-expected.txt" "$NEVO_QUIT_TMP/quit-output.txt"

./run.sh tests/fixtures/kaboom_source.n "$NEVO_QUIT_TMP/kaboom-test"
"$NEVO_QUIT_TMP/kaboom-test" > "$NEVO_QUIT_TMP/kaboom-output.txt"
test ! -s "$NEVO_QUIT_TMP/kaboom-output.txt"

echo "v1.3 quit and kaboom tests passed"
