#!/usr/bin/env bash
# Executes a Linux-native build + full test suite inside an ubuntu:24.04 Docker
# container — so the cross-platform build is actually RUN, not just defined in
# CI. Verified: builds clean and passes all tests against Ubuntu's Qt 6.4 +
# OpenSSL 3.0 (which exercises the scrypt KDF fallback and the fill()/pty.h
# portability paths). Requires Docker.
#
# Usage: scripts/linux-build.sh
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
SCRIPT="$(mktemp)"
cat > "$SCRIPT" <<'EOF'
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq cmake g++ ninja-build qt6-base-dev libqt6serialport6-dev \
    qt6-base-dev-tools libvterm-dev libssh2-1-dev libssl-dev
cd /repo
cmake -S . -B /tmp/lbuild -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build /tmp/lbuild --parallel
cd /tmp/lbuild && QT_QPA_PLATFORM=offscreen ctest --output-on-failure
EOF

docker run --rm -v "$REPO":/repo:ro -v "$SCRIPT":/build.sh:ro ubuntu:24.04 bash /build.sh
rm -f "$SCRIPT"
