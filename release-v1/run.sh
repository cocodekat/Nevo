#!/bin/bash

if [ $# -ne 2 ]; then
    echo "Usage: $0 <input.n> <output_name>"
    exit 1
fi

INPUT="$1"
OUTNAME="$2"

$HOME/nevo/compiler "$INPUT"
clang "$HOME/nevo/output.s" -o "$OUTNAME"