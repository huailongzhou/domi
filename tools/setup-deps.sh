#!/usr/bin/env bash
# Setup third-party dependencies for Domi Engine.
# Run from the project root:
#   bash tools/setup-deps.sh
#
# To use a GitHub mirror/proxy, set GITHUB_PROXY:
#   GITHUB_PROXY=https://ghproxy.com/ bash tools/setup-deps.sh
# To also install Emscripten SDK:
#   SETUP_EMSDK=1 bash tools/setup-deps.sh

set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
SDL_VERSION="3.2.14"
WAMR_VERSION="2.4.3"

# Optional GitHub proxy/mirror prefix.
PROXY="${GITHUB_PROXY:-}"

download() {
    local url="$1"
    local out="$2"
    if [ -f "$out" ]; then
        echo "[setup] Using existing $out"
        return 0
    fi
    echo "[setup] Downloading $out"
    curl -fsSL -o "$out" "${PROXY}${url}"
}

# ------------------------------------------------------------------
# SDL3
# ------------------------------------------------------------------
SDL_ARCHIVE="SDL3-${SDL_VERSION}.tar.gz"
SDL_SRC="SDL3-${SDL_VERSION}"
SDL_URL="https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/${SDL_ARCHIVE}"

if [ ! -d "$SDL_SRC" ]; then
    download "$SDL_URL" "$SDL_ARCHIVE"
    tar xzf "$SDL_ARCHIVE"
fi

mkdir -p sdl3-build
cd sdl3-build
cmake "../$SDL_SRC" \
    -DCMAKE_INSTALL_PREFIX="$ROOT/sdl3" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DSDL_EXAMPLES=OFF \
    -DSDL_TESTS=OFF
cmake --build . -j"$JOBS"
cmake --install .
cd ..

# ------------------------------------------------------------------
# WAMR (iwasm)
# ------------------------------------------------------------------
WAMR_ARCHIVE="WAMR-${WAMR_VERSION}.tar.gz"
WAMR_SRC="wasm-micro-runtime-WAMR-${WAMR_VERSION}"
WAMR_URL="https://github.com/bytecodealliance/wasm-micro-runtime/archive/refs/tags/WAMR-${WAMR_VERSION}.tar.gz"

if [ ! -d "$WAMR_SRC" ]; then
    download "$WAMR_URL" "$WAMR_ARCHIVE"
    tar xzf "$WAMR_ARCHIVE"
fi

mkdir -p wamr-build
cd wamr-build
cmake "../$WAMR_SRC/product-mini/platforms/darwin" \
    -DCMAKE_INSTALL_PREFIX="$ROOT/wamr" \
    -DWAMR_BUILD_PLATFORM=darwin \
    -DWAMR_BUILD_INTERP=1 \
    -DWAMR_BUILD_FAST_INTERP=1 \
    -DWAMR_BUILD_AOT=1 \
    -DWAMR_BUILD_SIMD=0
cmake --build . -j"$JOBS"
cmake --install .
cd ..

# ------------------------------------------------------------------
# Emscripten SDK (optional)
# ------------------------------------------------------------------
if [ "$SETUP_EMSDK" = "1" ]; then
    if [ ! -d "emsdk" ]; then
        echo "[setup] Cloning Emscripten SDK"
        git clone https://github.com/emscripten-core/emsdk.git
    fi
    cd emsdk
    ./emsdk install latest
    ./emsdk activate latest
    cd ..
fi

echo "[setup] Done. Dependencies installed to:"
echo "  SDL3: $ROOT/sdl3"
echo "  WAMR: $ROOT/wamr"
[ "$SETUP_EMSDK" = "1" ] && echo "  Emscripten: $ROOT/emsdk"
