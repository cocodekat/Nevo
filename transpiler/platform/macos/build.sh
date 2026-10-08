#!/usr/bin/env bash
set -euo pipefail

SOURCE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUTPUT_DIR="${1:-"$SOURCE_ROOT/build/macos"}"

mkdir -p "$OUTPUT_DIR/Modes" "$OUTPUT_DIR/Libraries"

clang "$SOURCE_ROOT/src/core/nevo.c" -o "$OUTPUT_DIR/nevo"
clang -I "$SOURCE_ROOT/include" \
  "$SOURCE_ROOT/src/modes/n.c" \
  "$SOURCE_ROOT/src/core/auto_var.c" \
  "$SOURCE_ROOT/src/core/ban_list.c" \
  -o "$OUTPUT_DIR/Modes/n"

cp -R "$SOURCE_ROOT/libraries/." "$OUTPUT_DIR/Libraries/"

echo "macOS transpiler build created at: $OUTPUT_DIR"
