#!/usr/bin/env bash
# Cross-platform build entry point for macOS and Linux (the Windows counterpart
# is scripts/build.ps1). Configures, builds, and runs the test suite.
set -euo pipefail

BUILD_TYPE="${1:-Debug}"
BUILD_DIR="${2:-build}"

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" --parallel

( cd "$BUILD_DIR" && QT_QPA_PLATFORM=offscreen ctest --output-on-failure )
echo "Build + tests complete ($BUILD_TYPE)."
