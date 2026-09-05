# Pasgen

A secure, cross-platform password manager. Stores credentials in a `.pif` database file encrypted twice over (ChaCha20-Poly1305 then AES-256-GCM) using auto-calibrated Argon2id key derivation. Features TOTP (2FA), password generation, and password history.

## Features

- Two-layer cipher cascade (ChaCha20-Poly1305 then AES-256-GCM) with a fully authenticated header
- Argon2id key derivation, auto-calibrated to the machine (PBKDF2-SHA256 fallback, recorded in the header)
- TOTP / authenticator app support (live code display with countdown timer)
- Secure password generator — random or passphrase mode
- Password history (last 10 passwords per account)
- Categories, custom user-defined fields, favourites and archiving
- Idle auto-lock and automatic clipboard clearing
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
| Argon2 | Not needed with OpenSSL 3.2+ (built in); `libargon2` used on older OpenSSL |

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

Pasgen uses the `.pif` binary format, **version 3**:

```
[4]  Magic: "PIF\0"
[2]  Version: 0x0003
[2]  Flags (reserved, authenticated)
[2]  KDF id      (1 = Argon2id, 2 = PBKDF2-SHA256)
[2]  Cipher id   (2 = ChaCha20-Poly1305 -> AES-256-GCM cascade)
[32] Salt (random, per-database)
[4]  Argon2 memory_kb
[4]  Argon2 iterations
[4]  Argon2 parallelism
[4]  PBKDF2 iterations
[12] Outer AES-GCM IV        (random, per-save)
[12] Inner ChaCha20 nonce    (random, per-save)
[4]  Encrypted payload length
[N]  Ciphertext
[16] Outer authentication tag
```

**The entire 88-byte header is the GCM AAD**, passed verbatim rather than
reconstructed, so every field above — version, reserved flags, KDF id, cipher
id and payload length — is covered by the authentication tag.

The KDF parameters and the payload length are validated against sane bounds
*before* being used, so a tampered header cannot request a multi-gigabyte
allocation or an absurd Argon2 memory cost.

### Cipher cascade

The payload is encrypted **twice, under two independent subkeys**:

```
plaintext
   |  ChaCha20-Poly1305   (key K1, inner nonce)
   v
inner ciphertext + inner tag
   |  AES-256-GCM         (key K2, outer IV)
   v
stored ciphertext + outer tag
```

The outer layer encrypts the inner ciphertext *and* its tag, so nothing about
the inner result is observable without first breaking AES-256-GCM. Recovering
the plaintext requires breaking **both** primitives — a future weakness in
either one alone does not expose the vault.

`K1` and `K2` are derived by HKDF-SHA512 from the Argon2id output under distinct
domain-separation labels, so neither cipher ever sees the master key and the two
keys are independent.

### Key derivation

Argon2id, **auto-calibrated on database creation** to roughly 750 ms on the
machine that creates it, capped at 512 MiB so the database stays openable on
more modest hardware. The chosen parameters are stored in the header, so a
faster machine produces a proportionally more expensive database to attack.

On a modern laptop this calibrates to **512 MiB / t=3 / p=8**, making each
password guess cost about **0.9 seconds** — and the 512 MiB memory cost is what
denies an attacker cheap GPU parallelism.

Argon2id comes from OpenSSL 3.2+ where available and falls back to `libargon2`
on older OpenSSL; the two produce byte-identical output, so databases stay
portable between builds. If neither is present the build uses PBKDF2-SHA256, and
because the KDF id is recorded in the header, a mismatch is reported plainly
instead of masquerading as a wrong password.

The derived key is cached while the database is unlocked, so saving does not
re-run the KDF.

### Operational hardening

Strong ciphers do not help if the vault is left open. Pasgen also:

- **Auto-locks after inactivity** (default 5 minutes, configurable in
  Preferences, `Ctrl+L` to lock immediately). Locking saves any pending change,
  then releases the master password and every decrypted account from memory.
- **Clears the clipboard** a configurable interval after a copy (default 30 s),
  and only if the contents are still the ones Pasgen put there.
- Writes saves to a temporary file and renames, so an interrupted save cannot
  leave a half-written vault.

---

## Usage

1. Launch `pasgen`
2. **Create** a new `.pif` database or **Open** an existing one
3. Enter your master password
4. Add, edit, and delete accounts
5. Press **Ctrl+S** or click **Save** to write changes to disk

### TOTP Setup

Paste the Base32 secret from your authenticator app's QR code setup screen into the **TOTP** field. The current 6-digit code and countdown timer are shown automatically.
