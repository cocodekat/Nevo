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
    tests/fixtures/windows_features_source.n \
    tests/fixtures/quit_source.n \
    tests/fixtures/kaboom_source.n \
    tests/fixtures/runtime_replace_source.n \
    tests/fixtures/logical_rand_source.n \
    tests/fixtures/input_source.n \
    tests/fixtures/print_exact_source.n \
    tests/fixtures/control_array_source.n \
    tests/fixtures/input_range_source.n \
    tests/fixtures/global_scope_source.n \
    tests/fixtures/terminal_string_source.n \
    tests/fixtures/slang_source.n \
    tests/fixtures/slang_failure.n \
    tests/fixtures/let_source.n \
    examples/all_features.n
do
    name="$(basename "$source" .n)"
    "$NEVO_WINDOWS_TMP/format" "$source" -o "$NEVO_WINDOWS_TMP/$name.json"
    "$NEVO_WINDOWS_TMP/codegen" "$NEVO_WINDOWS_TMP/$name.json" "$NEVO_WINDOWS_TMP/$name.asm"
    nasm -f win64 "$NEVO_WINDOWS_TMP/$name.asm" -o "$NEVO_WINDOWS_TMP/$name.obj"
done

file "$NEVO_WINDOWS_TMP/return_source.obj" | grep -q "Intel amd64 COFF"
grep -q 'gv_score' "$NEVO_WINDOWS_TMP/global_scope_source.asm"
if grep -q 'gv_x' "$NEVO_WINDOWS_TMP/global_scope_source.asm"; then
    echo 'ordinary Windows declaration unexpectedly used global storage' >&2
    exit 1
fi
echo "v1.4 Windows NASM code generation tests passed"
