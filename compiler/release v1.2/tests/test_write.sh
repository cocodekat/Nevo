#!/usr/bin/env bash

set -e
cd "$(dirname "$0")/.."

NEVO_TEST_TMP="$(mktemp -d)"
cleanup() {
    rm -f tests/fixtures/work-target.txt tests/fixtures/work-created.txt
    rm -rf "$NEVO_TEST_TMP"
}
trap cleanup EXIT

cp tests/fixtures/target.txt tests/fixtures/work-target.txt
./run.sh tests/fixtures/write_source.n "$NEVO_TEST_TMP/write-test"
"$NEVO_TEST_TMP/write-test" > "$NEVO_TEST_TMP/write-output.txt"
diff -u tests/fixtures/write_expected.txt "$NEVO_TEST_TMP/write-output.txt"

test "$(cat tests/fixtures/work-created.txt)" = "created"
printf 'hello\nS1\nS2\nthree' | cmp -s - tests/fixtures/work-target.txt
echo "v1.2 file write tests passed"
