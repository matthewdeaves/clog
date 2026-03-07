#!/bin/bash
# clog Development Environment Setup
# Builds clog for all available targets and registers CLOG_DIR in ~/.bashrc
#
# Works on Ubuntu 24/25 and macOS with Homebrew.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RETRO68_TOOLCHAIN="${RETRO68_TOOLCHAIN:-$HOME/Retro68-build/toolchain}"

echo "clog Development Setup"
echo "======================"
echo ""

# ── Helpers ──────────────────────────────────────────────────────

check_tool() {
    if command -v "$1" &>/dev/null; then
        echo "  [ok] $1 ($(command -v "$1"))"
        return 0
    else
        echo "  [!!] $1 not found"
        return 1
    fi
}

ensure_bashrc_export() {
    local var_name="$1" var_value="$2"
    if ! grep -q "export ${var_name}=" "$HOME/.bashrc" 2>/dev/null; then
        echo "" >> "$HOME/.bashrc"
        echo "# Added by clog/setup.sh" >> "$HOME/.bashrc"
        echo "export ${var_name}=\"${var_value}\"" >> "$HOME/.bashrc"
        echo "  [ok] Added ${var_name} to ~/.bashrc"
    else
        echo "  [ok] ${var_name} already in ~/.bashrc"
    fi
}

install_packages() {
    if command -v apt-get &>/dev/null; then
        sudo apt-get install -y "$@"
    elif command -v brew &>/dev/null; then
        brew install "$@"
    else
        echo "  [!!] No supported package manager found (need apt or brew)"
        exit 1
    fi
}

# ── Check core prerequisites ────────────────────────────────────

echo "Checking prerequisites..."
MISSING=0
check_tool gcc || MISSING=1
check_tool cmake || MISSING=1
check_tool make || MISSING=1

if [ "$MISSING" -eq 1 ]; then
    echo ""
    echo "Missing core tools. Install with:"
    if command -v apt-get &>/dev/null; then
        echo "  sudo apt install build-essential cmake"
    elif command -v brew &>/dev/null; then
        echo "  brew install gcc cmake make"
    fi
    exit 1
fi

# ── Build POSIX target ──────────────────────────────────────────

echo ""
echo "Building POSIX target..."
mkdir -p "$SCRIPT_DIR/build"
if (cd "$SCRIPT_DIR/build" && cmake .. && make && ctest --output-on-failure) 2>&1; then
    echo "  [ok] POSIX build + tests passed"
else
    echo "  [!!] POSIX build failed"
    exit 1
fi

# ── Build cross-compile targets (optional) ──────────────────────

echo ""
echo "Checking Retro68 cross-compiler..."

if [ -x "$RETRO68_TOOLCHAIN/bin/m68k-apple-macos-gcc" ]; then
    echo "  [ok] m68k-apple-macos-gcc"

    echo ""
    echo "Building 68k target..."
    M68K_TOOLCHAIN="$RETRO68_TOOLCHAIN/m68k-apple-macos/cmake/retro68.toolchain.cmake"
    mkdir -p "$SCRIPT_DIR/build-m68k"
    if (cd "$SCRIPT_DIR/build-m68k" && cmake .. -DCMAKE_TOOLCHAIN_FILE="$M68K_TOOLCHAIN" && make) 2>&1; then
        echo "  [ok] 68k build succeeded"
    else
        echo "  [!!] 68k build failed"
    fi
else
    echo "  [--] m68k-apple-macos-gcc not found (68k builds unavailable)"
fi

if [ -x "$RETRO68_TOOLCHAIN/bin/powerpc-apple-macos-gcc" ]; then
    echo "  [ok] powerpc-apple-macos-gcc"

    echo ""
    echo "Building PPC target..."
    PPC_TOOLCHAIN="$RETRO68_TOOLCHAIN/powerpc-apple-macos/cmake/retroppc.toolchain.cmake"
    mkdir -p "$SCRIPT_DIR/build-ppc"
    if (cd "$SCRIPT_DIR/build-ppc" && cmake .. -DCMAKE_TOOLCHAIN_FILE="$PPC_TOOLCHAIN" && make) 2>&1; then
        echo "  [ok] PPC build succeeded"
    else
        echo "  [!!] PPC build failed"
    fi
else
    echo "  [--] powerpc-apple-macos-gcc not found (PPC builds unavailable)"
fi

# ── Register CLOG_DIR in ~/.bashrc ──────────────────────────────

echo ""
echo "Checking environment..."
ensure_bashrc_export "CLOG_DIR" "$SCRIPT_DIR"

export CLOG_DIR="$SCRIPT_DIR"

# ── Summary ─────────────────────────────────────────────────────

echo ""
echo "======================"
echo "Setup complete!"
echo ""
echo "  CLOG_DIR:        $SCRIPT_DIR"
echo "  POSIX library:   $SCRIPT_DIR/build/libclog.a"
[ -f "$SCRIPT_DIR/build-m68k/libclog.a" ] && \
echo "  68k library:     $SCRIPT_DIR/build-m68k/libclog.a"
[ -f "$SCRIPT_DIR/build-ppc/libclog.a" ] && \
echo "  PPC library:     $SCRIPT_DIR/build-ppc/libclog.a"
echo ""
echo "Run 'source ~/.bashrc' to pick up CLOG_DIR in this shell."
echo ""
echo "Next step: set up peertalk (https://github.com/matthewdeaves/peertalk)"
echo ""
