#!/usr/bin/env bash
set -e

# Pasgen – macOS installer
# Installs dependencies via Homebrew and downloads the latest release binary.

REPO="YOUR_GITHUB_USERNAME/pasgen"
ARCH=$(uname -m)
if [ "$ARCH" = "arm64" ]; then
    RELEASE_URL="https://github.com/${REPO}/releases/latest/download/pasgen-macos-arm64.tar.gz"
else
    RELEASE_URL="https://github.com/${REPO}/releases/latest/download/pasgen-macos-x64.tar.gz"
fi
INSTALL_DIR="/usr/local/bin"

echo "=== Pasgen macOS Installer (${ARCH}) ==="

# ── Homebrew ──────────────────────────────────────────────────────────────────
if ! command -v brew &>/dev/null; then
    echo "Homebrew not found. Installing..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi

# ── Runtime dependencies ──────────────────────────────────────────────────────
echo "Installing dependencies via Homebrew..."
brew install openssl@3 sdl2 argon2

# ── Download and install binary ───────────────────────────────────────────────
TMP=$(mktemp -d)
trap "rm -rf $TMP" EXIT

echo "Downloading pasgen..."
curl -L --fail -o "$TMP/pasgen.tar.gz" "$RELEASE_URL"
tar -xzf "$TMP/pasgen.tar.gz" -C "$TMP"

# Remove quarantine attribute (prevents "damaged app" warning)
xattr -d com.apple.quarantine "$TMP/pasgen" 2>/dev/null || true
sudo install -m 755 "$TMP/pasgen" "$INSTALL_DIR/pasgen"

echo ""
echo "Pasgen installed to $INSTALL_DIR/pasgen"
echo "Run: pasgen"
