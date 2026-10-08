#!/usr/bin/env bash

set -e

usage() {
    echo "Usage: $0 <source.n> [source.n ...] -o <output>" >&2
    echo "Legacy: $0 <source.n> <output>" >&2
}

SOURCES=()
OUTPUT=""

if [ "$#" -eq 2 ] && [ "$1" != "-o" ] && [ "$2" != "-o" ]; then
    SOURCES=("$1")
    OUTPUT="$2"
else
    while [ "$#" -gt 0 ]; do
        case "$1" in
            -o)
                shift
                if [ "$#" -eq 0 ] || [ -n "$OUTPUT" ]; then
                    usage
                    exit 1
                fi
                OUTPUT="$1"
                ;;
            *)
                SOURCES+=("$1")
                ;;
        esac
        shift
    done
fi

if [ "${#SOURCES[@]}" -eq 0 ] || [ -z "$OUTPUT" ]; then
    usage
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_TMP="$(mktemp -d)"
cleanup() {
    rm -rf "$BUILD_TMP"
}
trap cleanup EXIT

FORMATTER="$BUILD_TMP/format"
CODEGEN="$BUILD_TMP/codegen"
AST="$BUILD_TMP/program.json"
ASSEMBLY="${OUTPUT}.asm"

clang -w -DNEVO_LIBRARY_BUILD \
    -I"$SCRIPT_DIR/include" \
    "$SCRIPT_DIR/src/format_main.c" "$SCRIPT_DIR/src/format.c" \
    "$SCRIPT_DIR/src/source_io.c" "$SCRIPT_DIR/src/source_graph.c" \
    -o "$FORMATTER"
"$FORMATTER" "${SOURCES[@]}" -o "$AST"

clang -w -DNEVO_LIBRARY_BUILD \
    -I"$SCRIPT_DIR/include" \
    "$SCRIPT_DIR/src/codegen_main.c" "$SCRIPT_DIR/src/codegen.c" \
    "$SCRIPT_DIR/src/source_io.c" \
    -o "$CODEGEN"
"$CODEGEN" "$AST" "$ASSEMBLY"

clang -arch arm64 "$ASSEMBLY" "$SCRIPT_DIR/src/file_runtime.c" -o "$OUTPUT"
