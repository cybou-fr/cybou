#!/usr/bin/env bash
# Builds the headless CYBOU tools (cybou, cybou-loadgen) inside WSL for the battle test.
set -euo pipefail
SRC=/mnt/c/Users/cybou/Desktop/cybou
DEPS=~/cybou-deps
if [ ! -f "$DEPS/blake3-1.8.1/lib/cmake/blake3/blake3Config.cmake" ]; then
    mkdir -p "$DEPS/src" && cd "$DEPS/src"
    [ -d BLAKE3 ] || git clone -q --depth 1 --branch 1.8.1 https://github.com/BLAKE3-team/BLAKE3.git
    cmake -S BLAKE3/c -B blake3-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$DEPS/blake3-1.8.1" -DCMAKE_POSITION_INDEPENDENT_CODE=ON >/dev/null
    cmake --build blake3-build >/dev/null && cmake --install blake3-build >/dev/null
fi
if [ ! -d ~/cybou/.git ]; then git clone -q "$SRC" ~/cybou; fi
cd ~/cybou && git pull -q "$SRC" main
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_GUI=OFF -DBUILD_TESTS=ON \
    -Dblake3_DIR="$DEPS/blake3-1.8.1/lib/cmake/blake3" >/dev/null
cmake --build build --target cybou cybou-loadgen
git log --oneline -1
