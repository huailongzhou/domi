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

if [ "$(uname -s)" = "Darwin" ]; then
    JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
    WAMR_PLATFORM="darwin"
else
    JOBS=$(nproc 2>/dev/null || echo 4)
    WAMR_PLATFORM="linux"
fi
SDL_VERSION="3.2.14"
WAMR_VERSION="2.4.3"
FREETYPE_VERSION="2.13.2"

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

if [ -f "$ROOT/sdl3/lib/libSDL3.dylib" ] || [ -f "$ROOT/sdl3/lib/libSDL3.so" ]; then
    echo "[setup] SDL3 already installed, skipping"
else
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
fi

# ------------------------------------------------------------------
# WAMR (iwasm)
# ------------------------------------------------------------------
WAMR_ARCHIVE="WAMR-${WAMR_VERSION}.tar.gz"
WAMR_SRC="wasm-micro-runtime-WAMR-${WAMR_VERSION}"
WAMR_URL="https://github.com/bytecodealliance/wasm-micro-runtime/archive/refs/tags/WAMR-${WAMR_VERSION}.tar.gz"

if [ -f "$ROOT/wamr/lib/libiwasm.a" ]; then
    echo "[setup] WAMR already installed, skipping"
else
    if [ ! -d "$WAMR_SRC" ]; then
        download "$WAMR_URL" "$WAMR_ARCHIVE"
        tar xzf "$WAMR_ARCHIVE"
    fi

    mkdir -p wamr-build
    cd wamr-build
    cmake "../$WAMR_SRC/product-mini/platforms/${WAMR_PLATFORM}" \
        -DCMAKE_INSTALL_PREFIX="$ROOT/wamr" \
        -DWAMR_BUILD_PLATFORM="${WAMR_PLATFORM}" \
        -DWAMR_BUILD_INTERP=1 \
        -DWAMR_BUILD_FAST_INTERP=1 \
        -DWAMR_BUILD_AOT=1 \
        -DWAMR_BUILD_SIMD=0
    cmake --build . -j"$JOBS"
    cmake --install .
    cd ..
fi

# ------------------------------------------------------------------
# FreeType
# ------------------------------------------------------------------
FREETYPE_ARCHIVE="freetype-${FREETYPE_VERSION}.tar.gz"
FREETYPE_SRC="freetype-${FREETYPE_VERSION}"
FREETYPE_URL="https://download.savannah.gnu.org/releases/freetype/${FREETYPE_ARCHIVE}"

if [ ! -d "$FREETYPE_SRC" ]; then
    download "$FREETYPE_URL" "$FREETYPE_ARCHIVE"
    tar xzf "$FREETYPE_ARCHIVE"
fi

# Native build
mkdir -p freetype-build
cd freetype-build
cmake "../$FREETYPE_SRC" \
    -DCMAKE_INSTALL_PREFIX="$ROOT/freetype" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DFT_DISABLE_ZLIB=ON \
    -DFT_DISABLE_BZIP2=ON \
    -DFT_DISABLE_PNG=ON \
    -DFT_DISABLE_HARFBUZZ=ON \
    -DFT_DISABLE_BROTLI=ON \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build . -j"$JOBS"
cmake --install .
cd ..

# MinGW build (optional; requires x86_64-w64-mingw32-gcc-posix)
if command -v x86_64-w64-mingw32-gcc-posix >/dev/null 2>&1; then
    mkdir -p freetype-mingw-build
    cd freetype-mingw-build
    cmake "../$FREETYPE_SRC" \
        -DCMAKE_INSTALL_PREFIX="$ROOT/freetype-mingw" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DFT_DISABLE_ZLIB=ON \
        -DFT_DISABLE_BZIP2=ON \
        -DFT_DISABLE_PNG=ON \
        -DFT_DISABLE_HARFBUZZ=ON \
        -DFT_DISABLE_BROTLI=ON \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc-posix \
        -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++-posix \
        -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    cmake --build . -j"$JOBS"
    cmake --install .
    cd ..
else
    echo "[setup] Skipping FreeType MinGW build (x86_64-w64-mingw32-gcc-posix not found)"
fi

# ------------------------------------------------------------------
# Default font asset
# ------------------------------------------------------------------
mkdir -p assets/fonts
FONT_OUT="assets/fonts/default.ttf"
if [ ! -f "$FONT_OUT" ]; then
    # Prefer a freely-licensed system font, then try common Linux paths.
    for sysfont in \
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf" \
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf" \
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf" \
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf"
    do
        if [ -f "$sysfont" ]; then
            echo "[setup] Using system font $sysfont"
            cp "$sysfont" "$FONT_OUT"
            break
        fi
    done
fi
if [ ! -f "$FONT_OUT" ]; then
    echo "[setup] Warning: no default font at $FONT_OUT"
    echo "[setup] Text rendering will not work until a TTF is placed there."
fi

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
echo "  SDL3:     $ROOT/sdl3"
echo "  WAMR:     $ROOT/wamr"
echo "  FreeType: $ROOT/freetype"
echo "  FreeType (MinGW): $ROOT/freetype-mingw"
echo "  Font:     $ROOT/$FONT_OUT"
if [ "$SETUP_EMSDK" = "1" ]; then
    echo "  Emscripten: $ROOT/emsdk"
fi
