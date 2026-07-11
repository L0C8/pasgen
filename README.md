# Pasgen

A secure, cross-platform password manager. Stores credentials in an AES-256-GCM encrypted `.pif` database file using Argon2id key derivation (with PBKDF2-SHA256 fallback). Features TOTP (2FA), password generation, and password history.

## Features

- AES-256-GCM encrypted database with authenticated header
- Argon2id key derivation (falls back to PBKDF2-SHA256 if Argon2 not available)
- TOTP / authenticator app support (live code display with countdown timer)
- Secure password generator — random or passphrase mode
- Password history (last 10 passwords per account)
- Cross-platform: Linux, macOS, Windows
- Zero GTK/Qt dependency — ImGui + SDL2 UI

---

## Download

| Platform | Download | Installer |
|----------|----------|-----------|
| **Linux x64** | [pasgen-linux-x64.tar.gz](https://github.com/YOUR_GITHUB_USERNAME/pasgen/releases/latest/download/pasgen-linux-x64.tar.gz) | `install-linux.sh` |
| **macOS (Apple Silicon)** | [pasgen-macos-arm64.tar.gz](https://github.com/YOUR_GITHUB_USERNAME/pasgen/releases/latest/download/pasgen-macos-arm64.tar.gz) | `install-mac.sh` |
| **macOS (Intel)** | [pasgen-macos-x64.tar.gz](https://github.com/YOUR_GITHUB_USERNAME/pasgen/releases/latest/download/pasgen-macos-x64.tar.gz) | `install-mac.sh` |
| **Windows x64** | [pasgen-windows-x64.zip](https://github.com/YOUR_GITHUB_USERNAME/pasgen/releases/latest/download/pasgen-windows-x64.zip) | `install-windows.bat` |

> **Note:** Replace `YOUR_GITHUB_USERNAME` above with your actual GitHub username after pushing this repo.

---

## Quick Install

### Linux
```bash
bash install-linux.sh
```

### macOS
```bash
bash install-mac.sh
```

### Windows
Double-click `install-windows.bat` or run from Command Prompt.

---

## Build from Source

### Requirements

| Dependency | Notes |
|------------|-------|
| CMake 3.16+ | |
| C++17 compiler | GCC 9+, Clang 9+, MSVC 2019+ |
| OpenSSL 1.1.1+ | `libssl-dev` / `openssl@3` / vcpkg |
| SDL2 | Optional — fetched automatically if not found |
| Argon2 | Optional — PBKDF2 fallback used if not found |

### Linux (Debian/Ubuntu)
```bash
sudo apt install build-essential cmake libssl-dev libsdl2-dev libargon2-dev
cd pasgen
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/pasgen
```

### macOS
```bash
brew install cmake openssl@3 sdl2 argon2
cd pasgen
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
./build/pasgen
```

### Windows (MSVC + vcpkg)
```powershell
vcpkg install openssl sdl2 argon2
cmake -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
.\build\Release\pasgen.exe
```

### Windows (MinGW/MSYS2)
```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-openssl mingw-w64-x86_64-SDL2
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
./build/pasgen.exe
```

> **First build:** CMake will automatically fetch ImGui, nlohmann/json, and portable-file-dialogs via FetchContent. An internet connection is required only for the first build.

---

## Database Format

Pasgen uses the `.pif` binary format (compatible with pasgen-classic):

```
[4]  Magic: "PIF\0"
[2]  Version: 0x0001
[2]  Flags (reserved)
[32] Salt (random, per-database)
[4]  Argon2 memory_kb
[4]  Argon2 iterations
[4]  Argon2 parallelism
[12] AES-GCM IV (random, per-save)
[4]  Encrypted payload length
[N]  AES-256-GCM ciphertext (JSON)
[16] GCM authentication tag
```

The header bytes (magic → parallelism) are included as AAD in the GCM tag, so any modification to the header will be detected.

---

## Usage

1. Launch `pasgen`
2. **Create** a new `.pif` database or **Open** an existing one
3. Enter your master password
4. Add, edit, and delete accounts
5. Press **Ctrl+S** or click **Save** to write changes to disk

### TOTP Setup

Paste the Base32 secret from your authenticator app's QR code setup screen into the **TOTP** field. The current 6-digit code and countdown timer are shown automatically.
# pasgen
