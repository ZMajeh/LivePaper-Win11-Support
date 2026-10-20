#!/usr/bin/env bash
#
# makeLinuxToWindow.sh - build LivePaper for Windows from Linux.
#
# Cross-compiles the project with the MinGW-w64 toolchain and packages the
# resulting executable together with the DLLs it needs, so that the folder can
# simply be copied to a Windows machine.
#
# Usage:
#   ./makeLinuxToWindow.sh [options]
#
# Options:
#   -o, --output DIR      output directory   (default: ./dist-windows)
#   -d, --deps DIR        dependency cache    (default: ~/.cache/livepaper-win)
#   -b, --build-dir DIR   CMake build directory (default: ./build-win)
#   -t, --build-type T    CMake build type    (default: Release)
#   -j, --jobs N          parallel build jobs (default: number of CPUs)
#       --deploy DIR      also copy the result to DIR (it can be given more
#                         than once; the default is the LivePaper folder on the
#                         Windows desktop, if it exists)
#       --clean           remove the build directory before compiling
#   -h, --help            show this help
#
# Examples:
#   ./makeLinuxToWindow.sh
#   ./makeLinuxToWindow.sh --output /tmp/win --build-type Debug
#
# Requirements (Debian/Ubuntu):
#   sudo apt-get install mingw-w64 cmake curl unzip tar zip
#
set -euo pipefail

# --------------------------------------------------------------------------
# Configuration
# --------------------------------------------------------------------------
SCRIPT_PATH="${BASH_SOURCE[0]}"
while [ -L "$SCRIPT_PATH" ]; do
    SCRIPT_DIR="$(cd -P "$(dirname "$SCRIPT_PATH")" && pwd)"
    SCRIPT_PATH="$(readlink "$SCRIPT_PATH")"
    [[ $SCRIPT_PATH != /* ]] && SCRIPT_PATH="$SCRIPT_DIR/$SCRIPT_PATH"
done
PROJECT_DIR="$(cd -P "$(dirname "$SCRIPT_PATH")" && pwd)"

SDL2_VERSION="2.30.12"
FFMPEG_BUILD="ffmpeg-n8.1-latest-win64-gpl-shared-8.1.zip"
SDL2_URL="https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-devel-${SDL2_VERSION}-mingw.tar.gz"
FFMPEG_URL="https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/${FFMPEG_BUILD}"

TARGET_TRIPLE="x86_64-w64-mingw32"
SDL2_ARCH_DIR="x86_64-w64-mingw32"

OUTPUT_DIR="$PROJECT_DIR/dist-windows"
DEPS_DIR="${LIVEPAPER_DEPS_DIR:-$HOME/.cache/livepaper-win}"
BUILD_DIR="$PROJECT_DIR/build-win"
BUILD_TYPE="Release"
JOBS="$( (command -v nproc >/dev/null 2>&1 && nproc) || echo 4)"
CLEAN=0
DEPLOY_DIRS=()

# The folder on the Windows desktop that the result is copied to by default.
DEFAULT_DEPLOY="/mnt/c/Users/mundh/OneDrive/Desktop/LivePaper-win64"

# --------------------------------------------------------------------------
# Helpers
# --------------------------------------------------------------------------
info()  { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
warn()  { printf '\033[1;33mwarning:\033[0m %s\n' "$*" >&2; }
error() { printf '\033[1;31merror:\033[0m %s\n' "$*" >&2; exit 1; }

usage() {
    sed -n '2,/^set -euo/p' "$SCRIPT_PATH" | sed 's/^# \{0,1\}//; $d'
    exit 0
}

# Downloads a file, resuming a partial download if there is one.
fetch() {
    local url="$1" out="$2"
    if [ -f "$out" ]; then
        info "$(basename "$out") is already downloaded"
        return 0
    fi
    info "downloading $(basename "$out")"
    if command -v curl >/dev/null 2>&1; then
        curl -fL --retry 5 --retry-delay 2 --continue-at - -o "$out" "$url" \
            || error "failed to download $url"
    elif command -v wget >/dev/null 2>&1; then
        wget -c -O "$out" "$url" || error "failed to download $url"
    else
        error "neither curl nor wget is available"
    fi
}

# --------------------------------------------------------------------------
# Command line
# --------------------------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        -o|--output)     OUTPUT_DIR="${2:?--output needs a directory}"; shift 2 ;;
        -d|--deps)       DEPS_DIR="${2:?--deps needs a directory}"; shift 2 ;;
        -b|--build-dir)  BUILD_DIR="${2:?--build-dir needs a directory}"; shift 2 ;;
        -t|--build-type) BUILD_TYPE="${2:?--build-type needs a value}"; shift 2 ;;
        -j|--jobs)       JOBS="${2:?--jobs needs a number}"; shift 2 ;;
        --deploy)        DEPLOY_DIRS+=("${2:?--deploy needs a directory}"); shift 2 ;;
        --clean)         CLEAN=1; shift ;;
        -h|--help)       usage ;;
        *)               error "unknown option '$1' (try --help)" ;;
    esac
done

# --------------------------------------------------------------------------
# Preflight checks
# --------------------------------------------------------------------------
info "checking the build prerequisites"

for tool in cmake tar unzip; do
    command -v "$tool" >/dev/null 2>&1 || error "'$tool' is not installed"
done
command -v "${TARGET_TRIPLE}-gcc" >/dev/null 2>&1 \
    || error "the MinGW-w64 compiler '${TARGET_TRIPLE}-gcc' is not installed.
       On Debian/Ubuntu:  sudo apt-get install mingw-w64"
command -v curl >/dev/null 2>&1 || command -v wget >/dev/null 2>&1 \
    || error "neither curl nor wget is available"

[ -f "$PROJECT_DIR/cmake/windows-mingw.cmake" ] \
    || error "the toolchain file cmake/windows-mingw.cmake is missing"

info "all prerequisites are present"

# --------------------------------------------------------------------------
# Dependencies
# --------------------------------------------------------------------------
mkdir -p "$DEPS_DIR"

SDL2_ARCHIVE="$DEPS_DIR/SDL2-mingw.tar.gz"
FFMPEG_ARCHIVE="$DEPS_DIR/ffmpeg-win64.zip"

fetch "$SDL2_URL" "$SDL2_ARCHIVE"
fetch "$FFMPEG_URL" "$FFMPEG_ARCHIVE"

# Verify the archives before extracting them.
if ! tar tzf "$SDL2_ARCHIVE" >/dev/null 2>&1; then
    rm -f "$SDL2_ARCHIVE"
    error "the SDL2 archive is corrupted, it has been removed; please run again"
fi
if ! unzip -t -q "$FFMPEG_ARCHIVE" >/dev/null 2>&1; then
    rm -f "$FFMPEG_ARCHIVE"
    error "the FFmpeg archive is corrupted, it has been removed; please run again"
fi

SDL2_DIR="$DEPS_DIR/sdl2/SDL2-${SDL2_VERSION}/${SDL2_ARCH_DIR}"
FFMPEG_DIR="$DEPS_DIR/ffmpeg"

if [ ! -d "$SDL2_DIR" ]; then
    info "extracting SDL2"
    rm -rf "$DEPS_DIR/sdl2"
    mkdir -p "$DEPS_DIR/sdl2"
    tar xzf "$SDL2_ARCHIVE" -C "$DEPS_DIR/sdl2"
fi
[ -d "$SDL2_DIR" ] || error "SDL2 was not extracted to $SDL2_DIR"

if [ ! -d "$FFMPEG_DIR/lib" ]; then
    info "extracting FFmpeg"
    rm -rf "$FFMPEG_DIR"
    mkdir -p "$FFMPEG_DIR"
    unzip -q -o "$FFMPEG_ARCHIVE" -d "$FFMPEG_DIR"
    # The archive contains a directory named after the build; move its
    # contents up one level so that ffmpeg/{bin,include,lib} is at the top.
    FFMPEG_INNER="$(find "$FFMPEG_DIR" -mindepth 1 -maxdepth 1 -type d | head -1)"
    if [ -n "$FFMPEG_INNER" ]; then
        mv "$FFMPEG_INNER"/* "$FFMPEG_DIR/"
        rmdir "$FFMPEG_INNER"
    fi
fi
[ -d "$FFMPEG_DIR/lib" ] || error "FFmpeg was not extracted to $FFMPEG_DIR"

# The import libraries are the ones that the linker actually consumes.
for lib in "$SDL2_DIR/lib/libSDL2.dll.a" "$SDL2_DIR/lib/libSDL2main.a"; do
    [ -f "$lib" ] || error "missing SDL2 import library: $lib"
done
for lib in avcodec avformat avutil swscale; do
    compgen -G "$FFMPEG_DIR/lib/lib${lib}.dll.a" >/dev/null \
        || error "missing FFmpeg import library for $lib in $FFMPEG_DIR/lib"
done

# --------------------------------------------------------------------------
# Build
# --------------------------------------------------------------------------
if [ "$CLEAN" -eq 1 ]; then
    info "removing the previous build directory"
    rm -rf "$BUILD_DIR"
fi

info "configuring ($BUILD_TYPE) for Windows"
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$PROJECT_DIR/cmake/windows-mingw.cmake" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DLIVEPAPER_SDL2_DIR="$SDL2_DIR" \
    -DLIVEPAPER_FFMPEG_DIR="$FFMPEG_DIR"

info "compiling with -j$JOBS"
cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" -j "$JOBS"

[ -f "$BUILD_DIR/live-paper.exe" ] || error "the build did not produce live-paper.exe"

# --------------------------------------------------------------------------
# Packaging
# --------------------------------------------------------------------------
info "collecting the binaries in $OUTPUT_DIR"
rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"
cp "$BUILD_DIR/live-paper.exe" "$OUTPUT_DIR/"
cp "$SDL2_DIR/bin/SDL2.dll" "$OUTPUT_DIR/"
cp "$FFMPEG_DIR"/bin/avcodec*.dll "$FFMPEG_DIR"/bin/avformat*.dll \
   "$FFMPEG_DIR"/bin/avutil*.dll "$FFMPEG_DIR"/bin/swresample*.dll \
   "$FFMPEG_DIR"/bin/swscale*.dll "$OUTPUT_DIR/"
cp "$PROJECT_DIR/assets/icon.ico" "$OUTPUT_DIR/"

# The console variant prints the diagnostics directly to the terminal, which
# is handy when the program is started from a command prompt.
if [ -f "$BUILD_DIR/live-paper-console.exe" ]; then
    cp "$BUILD_DIR/live-paper-console.exe" "$OUTPUT_DIR/"
fi

# A PowerShell script that prints the structure of the desktop windows; it is
# used to work out how the desktop is composed.
if [ -f "$PROJECT_DIR/tools/dump-windows.ps1" ]; then
    cp "$PROJECT_DIR/tools/dump-windows.ps1" "$OUTPUT_DIR/"
fi

# The archive is written next to the output folder.
ARCHIVE="$(cd "$(dirname "$OUTPUT_DIR")" && pwd)/LivePaper-win64.zip"
if command -v zip >/dev/null 2>&1; then
    rm -f "$ARCHIVE"
    (cd "$OUTPUT_DIR" && zip -q -r "$ARCHIVE" .)
    info "created $ARCHIVE"
else
    warn "'zip' is not installed, skipping the archive"
fi

# Copy the result to the folders that were requested. The Windows desktop
# folder is used by default, so that the new build is always there.
if [ ${#DEPLOY_DIRS[@]} -eq 0 ] && [ -d "$DEFAULT_DEPLOY" ]; then
    DEPLOY_DIRS+=("$DEFAULT_DEPLOY")
fi

# Copies a file, retrying: a file that is in use (a running program, or the
# synchronisation of the cloud storage) can't be overwritten at once.
copy_with_retry() {
    local src="$1" dest="$2"
    for attempt in 1 2 3 4 5; do
        if cp -f "$src" "$dest" 2>/dev/null; then
            return 0
        fi
        if cat "$src" > "$dest" 2>/dev/null; then
            return 0
        fi
        sleep 2
    done
    return 1
}

for DEPLOY_DIR in "${DEPLOY_DIRS[@]}"; do
    info "copying the result to $DEPLOY_DIR"
    mkdir -p "$DEPLOY_DIR"
    # The files are overwritten, but the directory itself is left alone: it
    # may be locked by the synchronisation of the cloud storage.
    for FILE in "$OUTPUT_DIR"/*; do
        NAME="$(basename "$FILE")"
        if ! copy_with_retry "$FILE" "$DEPLOY_DIR/$NAME"; then
            warn "can't copy $NAME to $DEPLOY_DIR (it is probably in use)"
        fi
    done
done

# --------------------------------------------------------------------------
# Summary
# --------------------------------------------------------------------------
printf '\n\033[1;32mBuild finished.\033[0m\n\n'
printf '  executable : %s\n' "$OUTPUT_DIR/live-paper.exe"
[ -f "$OUTPUT_DIR/live-paper-console.exe" ] && \
    printf '  console    : %s\n' "$OUTPUT_DIR/live-paper-console.exe"
[ -f "$ARCHIVE" ] && printf '  archive    : %s\n' "$ARCHIVE"
printf '\nCopy the whole folder (or the archive) to a Windows x64 machine and\n'
printf 'run live-paper.exe. The DLLs next to the executable are required.\n\n'
printf 'Both executables print their diagnostics to the terminal they are\n'
printf 'started from. Use --verbose for details and --stats for a periodic\n'
printf 'summary; --log-file=<path> also writes them to a file.\n\n'

ls -la "$OUTPUT_DIR"
