#!/usr/bin/env bash

set -e  # stop on first error

if [ $# -ne 2 ]; then
    echo "Usage: $0 <input.n> <output_base>"
    exit 1
fi

INPUT="$1"
OUT="$2"

JSON="${OUT}.json"
ASM="${OUT}.asm"
BIN="${OUT}"

# ---------------- Build formatter ----------------
clang -w format.c -o format

# ---------------- Run formatter ----------------
./format "$INPUT" "$JSON"

# ---------------- Build codegen ----------------
clang -w codegen.c -o codegen

# ---------------- Run codegen ----------------
./codegen "$JSON" "$ASM"

# ---------------- Final compile (ARM64) ----------------
clang -arch arm64 "$ASM" -o "$BIN"


rm "$JSON"
rm codegen
rm format