#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TARGET_DIR="${NEVO_HOME:-"$HOME/nevo"}"

echo "Building Nevo transpiler for macOS..."
"$SCRIPT_DIR/build.sh" "$TARGET_DIR"

echo "Installation complete: $TARGET_DIR"
