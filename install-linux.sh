#!/usr/bin/env bash
set -e

# Pasgen – Linux installer
# Downloads the latest release binary and installs system dependencies.

REPO="YOUR_GITHUB_USERNAME/pasgen"
RELEASE_URL="https://github.com/${REPO}/releases/latest/download/pasgen-linux-x64.tar.gz"
INSTALL_DIR="/usr/local/bin"

echo "=== Pasgen Linux Installer ==="

# ── Install runtime dependencies ──────────────────────────────────────────────
echo "Installing runtime dependencies..."

if command -v apt-get &>/dev/null; then
    sudo apt-get update -q
    sudo apt-get install -y libssl3 libsdl2-2.0-0 libargon2-1
elif command -v dnf &>/dev/null; then
    sudo dnf install -y openssl-libs SDL2 libargon2
elif command -v pacman &>/dev/null; then
    sudo pacman -Sy --noconfirm openssl sdl2 argon2
elif command -v zypper &>/dev/null; then
    sudo zypper install -y libopenssl3 libSDL2-2_0-0 libargon2-1
else
    echo "WARNING: Unknown package manager. Please install libssl, libSDL2, and libargon2 manually."
fi

# ── Download and install binary ───────────────────────────────────────────────
TMP=$(mktemp -d)
trap "rm -rf $TMP" EXIT

echo "Downloading pasgen..."
if command -v curl &>/dev/null; then
    curl -L --fail -o "$TMP/pasgen.tar.gz" "$RELEASE_URL"
elif command -v wget &>/dev/null; then
    wget -q -O "$TMP/pasgen.tar.gz" "$RELEASE_URL"
else
    echo "ERROR: Neither curl nor wget found. Please install one of them."
    exit 1
fi

tar -xzf "$TMP/pasgen.tar.gz" -C "$TMP"
sudo install -m 755 "$TMP/pasgen" "$INSTALL_DIR/pasgen"

echo ""
echo "Pasgen installed to $INSTALL_DIR/pasgen"
echo "Run: pasgen"
