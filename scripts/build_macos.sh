#!/usr/bin/env bash
# OpenStrike — build helper for macOS (Apple Silicon / Intel)
#
# Installs nothing destructive; only checks for dependencies and configures
# a CMake build in ./build, then compiles.
#
# Usage:
#   ./scripts/build_macos.sh            # configure + build (Release)
#   ./scripts/build_macos.sh debug      # debug build with symbols

set -euo pipefail

BUILD_TYPE="RelWithDebInfo"
if [[ "${1:-}" == "debug" ]]; then
    BUILD_TYPE="Debug"
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

echo "==> OpenStrike macOS build (${BUILD_TYPE})"

# ── 1. Check toolchain ───────────────────────────────────────────────────────
if ! command -v cmake >/dev/null 2>&1; then
    echo "ERROR: cmake not found. Install with:  brew install cmake"
    exit 1
fi

if ! command -v brew >/dev/null 2>&1; then
    echo "WARNING: Homebrew not found. If SDL2 is missing the build will fail."
    echo "         Install Homebrew from https://brew.sh"
fi

# ── 2. Check SDL2 ────────────────────────────────────────────────────────────
if ! pkg-config --exists sdl2 2>/dev/null && [[ ! -d /opt/homebrew/include/SDL2 ]] \
        && [[ ! -d /usr/local/include/SDL2 ]]; then
    echo "==> SDL2 not detected. Installing via Homebrew..."
    brew install sdl2
fi

# Help CMake find Homebrew SDL2 on Apple Silicon (/opt/homebrew) and Intel (/usr/local)
BREW_PREFIX="$(brew --prefix 2>/dev/null || echo /opt/homebrew)"
export CMAKE_PREFIX_PATH="${BREW_PREFIX}:${CMAKE_PREFIX_PATH:-}"

# ── 3. Configure ─────────────────────────────────────────────────────────────
echo "==> Configuring (CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH})"
cmake -S . -B build \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
      -DOSTRIKE_BUILD_TESTS=ON

# ── 4. Build ─────────────────────────────────────────────────────────────────
echo "==> Building"
cmake --build build -j"$(sysctl -n hw.ncpu)"

echo ""
echo "==> Done. Binary at: build/openstrike"
echo ""
echo "Run on your CS 1.6 assets:"
echo '  ./build/openstrike \'
echo '    --cs-dir "/Users/vladimirtitskiy/Library/Application Support/Steam/steamapps/common/Half-Life" \'
echo '    --map de_dust2'
