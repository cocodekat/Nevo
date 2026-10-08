#!/bin/bash

# ── Detect the directory where this script is ──
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

TARGET_DIR="$HOME/nevo"

mkdir -p $HOME/nevo

# ── Compiling the compiler
echo "Compiling Nevo Compiler..."
clang "../compiler.c" "../errors.c" -o compiler
clang "../transpiler.c" -o transpiler

mv compiler "$TARGET_DIR/"
mv transpiler "$TARGET_DIR/"
mv ../run.sh "$TARGET_DIR"

# ── Done!
echo "✅ Done! Everything You Need Is Inside Of $TARGET_DIR"