#!/usr/bin/env bash
# Cross-compiles the Windows ConPTY code path for a Windows target from any host
# (uses MinGW-w64). Proves the platform/Pty.cpp Windows branch is valid Win32
# without needing a Windows machine. Produces conpty_check.exe (a real PE binary).
#
#   brew install mingw-w64   # or: apt-get install g++-mingw-w64-x86-64
#   scripts/win/build-check.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
CXX="${MINGW_CXX:-x86_64-w64-mingw32-g++}"

"$CXX" -std=c++17 -Wall -o "$HERE/conpty_check.exe" "$HERE/conpty_check.cpp"
echo "OK: cross-compiled conpty_check.exe (Windows x86_64 PE)"
file "$HERE/conpty_check.exe" 2>/dev/null || true
