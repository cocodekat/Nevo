#!/bin/bash
# Simple installer script: downloads a zip, extracts it, runs a command, and cleans up

# -----------------------------
# Hardcoded variables
# -----------------------------
ZIP_URL="https://github.com/cocodekat/nevo-lang/raw/main/versions/nevo-v1.zip?download=1"
DOWNLOADS_DIR="$HOME/Downloads"
ZIP_NAME="nevo.zip"
EXTRACT_DIR="nevo"
INSTALLER_PATH="$DOWNLOADS_DIR/$EXTRACT_DIR/nevo-v1/installer/install.sh"

# -----------------------------
# Download ZIP
# -----------------------------
echo "📥 Downloading zip from GitHub..."
curl -L "$ZIP_URL" -o "$DOWNLOADS_DIR/$ZIP_NAME"

if [ $? -ne 0 ]; then
    echo "❌ Failed to download ZIP."
    exit 1
fi

# -----------------------------
# Extract ZIP
# -----------------------------
echo "📂 Extracting ZIP..."
mkdir -p "$DOWNLOADS_DIR/$EXTRACT_DIR"
unzip -q "$DOWNLOADS_DIR/$ZIP_NAME" -d "$DOWNLOADS_DIR/$EXTRACT_DIR"

if [ $? -ne 0 ]; then
    echo "❌ Failed to extract ZIP."
    exit 1
fi

# -----------------------------
# Run installer
# -----------------------------
echo "⚡ Running installer..."
if [ -f "$INSTALLER_PATH" ]; then
    chmod +x "$INSTALLER_PATH"
    "$INSTALLER_PATH"
else
    echo "❌ Installer not found at $INSTALLER_PATH"
    exit 1
fi

# -----------------------------
# Cleanup (optional)
# -----------------------------
echo "🧹 Cleaning up..."
rm -rf "$DOWNLOADS_DIR/$ZIP_NAME" "$DOWNLOADS_DIR/$EXTRACT_DIR"
