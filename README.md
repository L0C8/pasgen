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

## Install

Pasgen builds from source. One script per platform installs any missing
dependencies, compiles the project, and installs the binary — nothing to set up
by hand.

```bash
git clone https://github.com/L0C8/pasgen.git
cd pasgen
```

| Platform | Install | Update |
|----------|---------|--------|
| **Linux** | `./install.sh` | `./update.sh` |
| **macOS** | `./install.sh` | `./update.sh` |
| **Windows** | `install.bat` (or `.\install.ps1`) | `update.bat` (or `.\update.ps1`) |

The installer detects what is already present and installs only what is
missing, so re-running it is cheap and safe.

- **Linux** — uses apt, dnf, pacman, zypper or apk, whichever the system has.
  Installs a C++ toolchain, CMake, pkg-config, Git, and the OpenSSL, SDL2,
  Argon2 and OpenGL development packages.
- **macOS** — installs the Xcode Command Line Tools and Homebrew if absent,
  then `cmake`, `openssl@3`, `sdl2` and `argon2`.
- **Windows** — installs Git, CMake and the MSVC C++ build tools through
  `winget`, then bootstraps vcpkg and builds OpenSSL, SDL2 and Argon2.

### Options

| Flag (Unix) | Flag (Windows) | Effect |
|-------------|----------------|--------|
| `--check` | `-Check` | Report dependency status and exit; change nothing |
| `--prefix DIR` | `-Prefix DIR` | Install somewhere else (default `/usr/local`, `%LOCALAPPDATA%\Programs\Pasgen`) |
| `--no-deps` | `-NoDeps` | Skip dependency installation entirely |
| `--clean` | `-Clean` | Delete the build tree before building |
| `--jobs N` | `-Jobs N` | Override the parallel build job count |

To install without root on Linux or macOS:

```bash
./install.sh --prefix "$HOME/.local"
```

---

## Updating

```bash
./update.sh          # Linux / macOS
update.bat           # Windows
```

The updater fast-forwards the checkout, then hands off to the installer, which
re-checks every dependency before rebuilding. Because the hand-off happens
*after* the pull, a dependency introduced by the commits just pulled is
installed on the same run.

```bash
./update.sh --check   # show what is available, change nothing
./update.sh --force   # rebuild and reinstall even if already current
```

The updater refuses to act on a detached HEAD, on a branch with no upstream, or
when local commits have diverged from the remote — it fast-forwards only and
will never discard your work.

> Prebuilt release binaries are not published yet, so the installers always
> build from source. The first build downloads ImGui, nlohmann/json and
> portable-file-dialogs, so it needs an internet connection.

---

## Manual Build

Prefer this only if you want to drive the build yourself — `install.sh` /
`install.bat` above do all of it for you.

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
sudo apt install build-essential cmake pkg-config \
    libssl-dev libsdl2-dev libargon2-dev libgl1-mesa-dev
cd pasgen
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/pasgen
```

### macOS
```bash
brew install cmake pkg-config openssl@3 sdl2 argon2
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
