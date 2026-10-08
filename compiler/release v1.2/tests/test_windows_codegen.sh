#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/.."

if ! command -v nasm >/dev/null 2>&1; then
    echo "NASM is required for the Windows backend test" >&2
    exit 1
fi

NEVO_WINDOWS_TMP="$(mktemp -d)"
cleanup() {
    rm -rf "$NEVO_WINDOWS_TMP"
}
trap cleanup EXIT

clang -w -DNEVO_LIBRARY_BUILD -Iinclude \
    src/format_main.c src/format.c src/source_io.c src/source_graph.c \
    -o "$NEVO_WINDOWS_TMP/format"
clang -w -DNEVO_LIBRARY_BUILD -DNEVO_TARGET_WINDOWS_X64 -Iinclude \
    src/codegen_main.c src/codegen.c src/source_io.c \
    -o "$NEVO_WINDOWS_TMP/codegen"

for source in \
    tests/fixtures/return_source.n \
    tests/fixtures/write_source.n \
    tests/fixtures/include_main.n \
    tests/fixtures/windows_abi_source.n \
    tests/fixtures/windows_features_source.n
do
    name="$(basename "$source" .n)"
    "$NEVO_WINDOWS_TMP/format" "$source" -o "$NEVO_WINDOWS_TMP/$name.json"
    "$NEVO_WINDOWS_TMP/codegen" "$NEVO_WINDOWS_TMP/$name.json" "$NEVO_WINDOWS_TMP/$name.asm"
    nasm -f win64 "$NEVO_WINDOWS_TMP/$name.asm" -o "$NEVO_WINDOWS_TMP/$name.obj"
done

file "$NEVO_WINDOWS_TMP/return_source.obj" | grep -q "Intel amd64 COFF"
echo "v1.2 Windows NASM code generation tests passed"
