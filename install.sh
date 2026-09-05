#!/usr/bin/env bash
#
# Pasgen — out-of-the-box installer for Linux and macOS.
#
# Checks what is already on the machine, installs only the dependencies that
# are actually missing, builds pasgen from source, and installs the binary
# together with its bundled fonts.
#
# Usage:
#   ./install.sh                     Build and install into /usr/local (uses sudo)
#   ./install.sh --prefix ~/.local   Install somewhere that needs no sudo
#   ./install.sh --no-deps           Assume dependencies are already present
#   ./install.sh --clean             Delete the build tree before building
#   ./install.sh --jobs 4            Override the parallel build job count
#   ./install.sh --check             Only report dependency status, change nothing
#
set -eo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$REPO_DIR/build"
PREFIX="/usr/local"
SKIP_DEPS=0
CLEAN=0
CHECK_ONLY=0
JOBS=""

# ── output helpers ───────────────────────────────────────────────────────────
if [ -t 1 ]; then
    BOLD=$'\033[1m'; GREEN=$'\033[32m'; YELLOW=$'\033[33m'; RED=$'\033[31m'; DIM=$'\033[2m'; RESET=$'\033[0m'
else
    BOLD=""; GREEN=""; YELLOW=""; RED=""; DIM=""; RESET=""
fi
say()   { printf '%s==>%s %s\n' "$BOLD" "$RESET" "$*"; }
ok()    { printf '    %s✓%s %s\n' "$GREEN" "$RESET" "$*"; }
miss()  { printf '    %s•%s %s\n' "$YELLOW" "$RESET" "$*"; }
note()  { printf '    %s%s%s\n' "$DIM" "$*" "$RESET"; }
warn()  { printf '%swarning:%s %s\n' "$YELLOW" "$RESET" "$*" >&2; }
die()   { printf '%serror:%s %s\n' "$RED" "$RESET" "$*" >&2; exit 1; }

# ── argument parsing ─────────────────────────────────────────────────────────
while [ $# -gt 0 ]; do
    case "$1" in
        --prefix) PREFIX="${2:?--prefix needs a directory}"; shift 2 ;;
        --prefix=*) PREFIX="${1#*=}"; shift ;;
        --jobs) JOBS="${2:?--jobs needs a number}"; shift 2 ;;
        --jobs=*) JOBS="${1#*=}"; shift ;;
        --no-deps) SKIP_DEPS=1; shift ;;
        --clean) CLEAN=1; shift ;;
        --check) CHECK_ONLY=1; shift ;;
        -h|--help) sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) die "unknown option: $1 (try --help)" ;;
    esac
done

# ── platform and package manager detection ───────────────────────────────────
detect_platform() {
    case "$(uname -s)" in
        Linux)  OS="linux" ;;
        Darwin) OS="macos" ;;
        *)      die "unsupported platform: $(uname -s). Use install.ps1 on Windows." ;;
    esac

    if [ "$OS" = "macos" ]; then
        PM="brew"
    elif command -v apt-get >/dev/null 2>&1; then PM="apt"
    elif command -v dnf     >/dev/null 2>&1; then PM="dnf"
    elif command -v pacman  >/dev/null 2>&1; then PM="pacman"
    elif command -v zypper  >/dev/null 2>&1; then PM="zypper"
    elif command -v apk     >/dev/null 2>&1; then PM="apk"
    else
        PM="none"
    fi
}

# Map a logical dependency to this platform's package name.
# An empty result means the dependency needs no package here.
pkg_name() {
    case "$PM:$1" in
        apt:compiler)     echo "build-essential" ;;
        apt:cmake)        echo "cmake" ;;
        apt:pkgconfig)    echo "pkg-config" ;;
        apt:git)          echo "git" ;;
        apt:openssl)      echo "libssl-dev" ;;
        apt:sdl2)         echo "libsdl2-dev" ;;
        apt:argon2)       echo "libargon2-dev" ;;
        apt:opengl)       echo "libgl1-mesa-dev" ;;

        dnf:compiler)     echo "gcc-c++ make" ;;
        dnf:cmake)        echo "cmake" ;;
        dnf:pkgconfig)    echo "pkgconf-pkg-config" ;;
        dnf:git)          echo "git" ;;
        dnf:openssl)      echo "openssl-devel" ;;
        dnf:sdl2)         echo "SDL2-devel" ;;
        dnf:argon2)       echo "libargon2-devel" ;;
        dnf:opengl)       echo "mesa-libGL-devel" ;;

        pacman:compiler)  echo "base-devel" ;;
        pacman:cmake)     echo "cmake" ;;
        pacman:pkgconfig) echo "base-devel" ;;
        pacman:git)       echo "git" ;;
        pacman:openssl)   echo "openssl" ;;
        pacman:sdl2)      echo "sdl2" ;;
        pacman:argon2)    echo "argon2" ;;
        pacman:opengl)    echo "mesa" ;;

        zypper:compiler)  echo "gcc-c++ make" ;;
        zypper:cmake)     echo "cmake" ;;
        zypper:pkgconfig) echo "pkg-config" ;;
        zypper:git)       echo "git" ;;
        zypper:openssl)   echo "libopenssl-devel" ;;
        zypper:sdl2)      echo "libSDL2-devel" ;;
        zypper:argon2)    echo "libargon2-devel" ;;
        zypper:opengl)    echo "Mesa-libGL-devel" ;;

        apk:compiler)     echo "build-base" ;;
        apk:cmake)        echo "cmake" ;;
        apk:pkgconfig)    echo "pkgconf" ;;
        apk:git)          echo "git" ;;
        apk:openssl)      echo "openssl-dev" ;;
        apk:sdl2)         echo "sdl2-dev" ;;
        apk:argon2)       echo "argon2-dev" ;;
        apk:opengl)       echo "mesa-dev" ;;

        # macOS: the compiler comes from the Xcode Command Line Tools and
        # OpenGL is a system framework, so neither maps to a formula.
        brew:compiler)    echo "" ;;
        brew:cmake)       echo "cmake" ;;
        brew:pkgconfig)   echo "pkg-config" ;;
        brew:git)         echo "git" ;;
        brew:openssl)     echo "openssl@3" ;;
        brew:sdl2)        echo "sdl2" ;;
        brew:argon2)      echo "argon2" ;;
        brew:opengl)      echo "" ;;

        *) echo "" ;;
    esac
}

# Human-readable label for the status report.
dep_label() {
    case "$1" in
        compiler)  echo "C++17 compiler" ;;
        cmake)     echo "CMake 3.16+" ;;
        pkgconfig) echo "pkg-config" ;;
        git)       echo "Git" ;;
        openssl)   echo "OpenSSL headers" ;;
        sdl2)      echo "SDL2 headers" ;;
        argon2)    echo "Argon2 headers" ;;
        opengl)    echo "OpenGL headers" ;;
        *)         echo "$1" ;;
    esac
}

# ── dependency detection ─────────────────────────────────────────────────────
cmake_new_enough() {
    local v major minor
    v="$(cmake --version 2>/dev/null | head -1 | awk '{print $3}')" || return 1
    major="${v%%.*}"; minor="${v#*.}"; minor="${minor%%.*}"
    [ -n "$major" ] || return 1
    [ "$major" -gt 3 ] && return 0
    [ "$major" -eq 3 ] && [ "$minor" -ge 16 ]
}

# A library counts as present if pkg-config sees it, or (on macOS) if Homebrew
# reports the formula installed — brew keeps openssl@3 keg-only and off the
# default pkg-config path.
lib_present() {
    local mod="$1" formula="$2"
    if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists "$mod" 2>/dev/null; then
        return 0
    fi
    if [ "$PM" = "brew" ] && [ -n "$formula" ] && command -v brew >/dev/null 2>&1; then
        brew list --versions "$formula" >/dev/null 2>&1 && return 0
    fi
    return 1
}

dep_present() {
    case "$1" in
        compiler)  command -v c++ >/dev/null 2>&1 || command -v g++ >/dev/null 2>&1 \
                       || command -v clang++ >/dev/null 2>&1 ;;
        cmake)     command -v cmake >/dev/null 2>&1 && cmake_new_enough ;;
        pkgconfig) command -v pkg-config >/dev/null 2>&1 ;;
        git)       command -v git >/dev/null 2>&1 ;;
        openssl)   lib_present openssl   "openssl@3" ;;
        sdl2)      lib_present sdl2      "sdl2" ;;
        argon2)    lib_present libargon2 "argon2" ;;
        opengl)    [ "$OS" = "macos" ] && return 0; lib_present gl "" ;;
        *)         return 1 ;;
    esac
}

# Echoes the subset of "$@" that is missing.
find_missing() {
    local d
    for d in "$@"; do
        dep_present "$d" || printf '%s\n' "$d"
    done
}

# ── package installation ─────────────────────────────────────────────────────
APT_UPDATED=0

install_packages() {
    local logical pkgs="" p
    for logical in "$@"; do
        p="$(pkg_name "$logical")"
        [ -n "$p" ] && pkgs="$pkgs $p"
    done
    # shellcheck disable=SC2086
    pkgs="$(printf '%s\n' $pkgs | sort -u | tr '\n' ' ')"
    [ -z "${pkgs// /}" ] && return 0

    if [ "$(id -u)" -ne 0 ] && [ -z "$SUDO" ] && [ "$PM" != "brew" ]; then
        die "these packages are missing but sudo is not available:$pkgs
       install them as root, or re-run with --no-deps once they are present"
    fi

    say "Installing:$pkgs"
    # shellcheck disable=SC2086
    case "$PM" in
        apt)
            if [ "$APT_UPDATED" -eq 0 ]; then $SUDO apt-get update -q; APT_UPDATED=1; fi
            $SUDO apt-get install -y $pkgs
            ;;
        dnf)    $SUDO dnf install -y $pkgs ;;
        pacman) $SUDO pacman -Sy --needed --noconfirm $pkgs ;;
        zypper) $SUDO zypper install -y $pkgs ;;
        apk)    $SUDO apk add $pkgs ;;
        brew)   brew install $pkgs ;;
        none)   die "no supported package manager found — install these manually:$pkgs" ;;
    esac
}

# ── macOS prerequisites ──────────────────────────────────────────────────────
ensure_macos_toolchain() {
    if ! xcode-select -p >/dev/null 2>&1; then
        say "Installing the Xcode Command Line Tools"
        note "Accept the dialog macOS opens, then re-run this script."
        xcode-select --install || true
        die "waiting on the Xcode Command Line Tools installation"
    fi
    if ! command -v brew >/dev/null 2>&1; then
        say "Installing Homebrew"
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        # Make brew usable in this shell for both Apple Silicon and Intel layouts.
        if   [ -x /opt/homebrew/bin/brew ]; then eval "$(/opt/homebrew/bin/brew shellenv)"
        elif [ -x /usr/local/bin/brew   ]; then eval "$(/usr/local/bin/brew shellenv)"
        fi
        command -v brew >/dev/null 2>&1 || die "Homebrew installed but 'brew' is not on PATH"
    fi
}

# ── privilege handling ───────────────────────────────────────────────────────
setup_sudo() {
    SUDO=""
    if [ "$(id -u)" -ne 0 ] && command -v sudo >/dev/null 2>&1; then
        SUDO="sudo"
    fi
    return 0
}

# Root must not own the build tree: a later non-root run would fail to write
# into it. This is the usual fallout from a first run under `sudo -i`.
check_build_dir_ownership() {
    [ -d "$BUILD_DIR" ] || return 0
    [ -w "$BUILD_DIR" ] && return 0
    warn "$BUILD_DIR is not writable by $(id -un) (likely built as root)."
    if [ -n "$SUDO" ] || [ "$(id -u)" -eq 0 ]; then
        say "Removing the unwritable build tree"
        $SUDO rm -rf "$BUILD_DIR"
    else
        die "remove it first:  sudo rm -rf '$BUILD_DIR'"
    fi
}

job_count() {
    if [ -n "$JOBS" ]; then echo "$JOBS"
    elif command -v nproc >/dev/null 2>&1; then nproc
    elif command -v sysctl >/dev/null 2>&1; then sysctl -n hw.ncpu 2>/dev/null || echo 4
    else echo 4
    fi
}

# ── main ─────────────────────────────────────────────────────────────────────
printf '%s\n' "${BOLD}=== Pasgen installer ===${RESET}"

detect_platform
setup_sudo

say "Platform"
ok "$OS ($(uname -m)), package manager: $PM"

if [ "$OS" = "macos" ] && [ "$(id -u)" -eq 0 ]; then
    die "do not run this script as root on macOS — Homebrew refuses to run as root"
fi

# Homebrew and the Xcode CLT have to exist before anything can be detected.
if [ "$OS" = "macos" ] && [ "$SKIP_DEPS" -eq 0 ] && [ "$CHECK_ONLY" -eq 0 ]; then
    ensure_macos_toolchain
fi

# Required to build at all.
REQUIRED_DEPS="compiler cmake pkgconfig git openssl"
[ "$OS" = "linux" ] && REQUIRED_DEPS="$REQUIRED_DEPS opengl"
# CMake can fall back without these (SDL2 gets fetched and built from source,
# Argon2 degrades to PBKDF2), but the build is far better off with them.
OPTIONAL_DEPS="sdl2 argon2"

say "Checking dependencies"
missing_required="$(find_missing $REQUIRED_DEPS || true)"
missing_optional="$(find_missing $OPTIONAL_DEPS || true)"

for d in $REQUIRED_DEPS $OPTIONAL_DEPS; do
    if dep_present "$d"; then ok "$(dep_label "$d")"; else miss "$(dep_label "$d") — missing"; fi
done

if [ "$CHECK_ONLY" -eq 1 ]; then
    if [ -z "$missing_required$missing_optional" ]; then
        say "Everything needed is already installed."
    else
        say "Run without --check to install what is missing."
    fi
    exit 0
fi

# ── install missing dependencies ─────────────────────────────────────────────
if [ "$SKIP_DEPS" -eq 1 ]; then
    [ -n "$missing_required" ] && warn "--no-deps given but required packages are missing; the build may fail"
elif [ -n "$missing_required$missing_optional" ]; then
    # pkg-config decides whether the library checks above are meaningful, so
    # install the base tools first and then re-test the libraries.
    base_missing="$(find_missing compiler cmake pkgconfig git || true)"
    if [ -n "$base_missing" ]; then
        install_packages $base_missing
    fi

    lib_missing="$(find_missing openssl sdl2 argon2 $([ "$OS" = "linux" ] && echo opengl) || true)"
    if [ -n "$lib_missing" ]; then
        install_packages $lib_missing
    fi

    say "Re-checking dependencies"
    still_missing="$(find_missing $REQUIRED_DEPS || true)"
    if [ -n "$still_missing" ]; then
        for d in $still_missing; do miss "$(dep_label "$d") — still missing"; done
        die "could not satisfy every required dependency; install the packages above by hand"
    fi
    ok "all required dependencies present"
else
    ok "nothing to install"
fi

# ── build ────────────────────────────────────────────────────────────────────
check_build_dir_ownership

if [ "$CLEAN" -eq 1 ] && [ -d "$BUILD_DIR" ]; then
    say "Cleaning $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

jobs="$(job_count)"
say "Configuring"
note "The first build downloads ImGui, nlohmann/json and portable-file-dialogs."
cmake -S "$REPO_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

say "Building with $jobs parallel jobs"
cmake --build "$BUILD_DIR" -j "$jobs"

# ── install ──────────────────────────────────────────────────────────────────
say "Installing to $PREFIX"
mkdir -p "$PREFIX" 2>/dev/null || true
if [ -w "$PREFIX" ] || { [ -d "$PREFIX/bin" ] && [ -w "$PREFIX/bin" ]; }; then
    cmake --install "$BUILD_DIR" --prefix "$PREFIX"
else
    [ -n "$SUDO" ] || die "$PREFIX is not writable and sudo is unavailable — try --prefix \"\$HOME/.local\""
    $SUDO cmake --install "$BUILD_DIR" --prefix "$PREFIX"
fi

printf '\n'
ok "pasgen installed to $PREFIX/bin/pasgen"
case ":$PATH:" in
    *":$PREFIX/bin:"*) note "Run: pasgen" ;;
    *) warn "$PREFIX/bin is not on your PATH. Add this to your shell profile:"
       note "export PATH=\"$PREFIX/bin:\$PATH\"" ;;
esac
note "Update later with: ./update.sh"
