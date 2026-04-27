#!/bin/bash

# ── Detect the directory where this script is ──
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

TARGET_DIR="$HOME/nevo"

# ── Building needed folders

# ── Compiling the compiler
echo "Compiling Nevo Compiler..."
clang "../compiler.c" "../errors.c" -o "$TARGET_DIR"
clang "../transpiler.c" -o "$TARGET_DIR"

# ── Moving the needed .h files
echo "Enabling Builtin Functions..."
mv "../libraries/errors.h" "$TARGET_DIR"

# ── Done!
echo "✅ Done! Everything You Need Is Inside Of $TARGET_DIR"
read -p "Press Enter To Continue..."
