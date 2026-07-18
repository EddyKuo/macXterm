#!/usr/bin/env bash
# Build + test macXterm on Linux (e.g. inside WSL) to verify the non-Windows code
# paths — the SAME forkpty / BSD-socket / Unix-shim branches macOS uses. A green
# run here is strong evidence this session's Windows work did not regress macOS.
#
# Usage (from Windows, once, providing your sudo password when prompted):
#     ! wsl bash /mnt/d/code/macXterm/scripts/verify-linux.sh
#
# It installs the dev packages (sudo apt), configures, builds, and runs ctest.
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO"

echo "== installing build dependencies (sudo apt) =="
sudo apt-get update -y
sudo apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build pkg-config \
    qt6-base-dev qt6-serialport-dev libgl1-mesa-dev \
    libssl-dev libvterm-dev libssh2-1-dev libssh-dev

echo "== configure =="
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release

echo "== build =="
cmake --build build-linux --parallel

echo "== test (offscreen) =="
cd build-linux
QT_QPA_PLATFORM=offscreen ctest --output-on-failure
echo "== DONE: if ctest reported 100% passed, the shared/Unix code paths are green =="
