#!/usr/bin/env bash
# Deploys a real RDP server fixture (FreeRDP's sfreerdp-server with a generated
# self-signed cert) and runs the guarded-live RDP e2e against it. Turns
# test_rdp_live from "SKIP" into a real end-to-end run — the FreeRDP client
# completes the RDP+TLS handshake, initializes the GDI, and captures a remote
# framebuffer. Verified passing on macOS. Requires FreeRDP (sfreerdp-server,
# winpr-makecert) and a FreeRDP-enabled build (MACXTERM_HAVE_FREERDP).
#
# Usage: scripts/rdp-fixture.sh [build-dir]   (default: ./build)
set -euo pipefail

BUILD="${1:-build}"
FX="$(mktemp -d)"
trap 'kill "$(cat "$FX/srv.pid" 2>/dev/null)" 2>/dev/null || true; rm -rf "$FX"' EXIT

winpr-makecert -rdp -n macxterm-test -path "$FX" >/dev/null 2>&1

sfreerdp-server --cert="$FX/macxterm-test.crt" --key="$FX/macxterm-test.key" \
                --port=3390 > "$FX/server.log" 2>&1 &
echo $! > "$FX/srv.pid"
sleep 2

# Fresh known-hosts so the self-signed cert path is exercised each run.
rm -f "$HOME/.config/freerdp/server/127.0.0.1_3390.pem" 2>/dev/null || true

export MACXTERM_RDP_TEST_HOST=127.0.0.1
export MACXTERM_RDP_TEST_PORT=3390
export QT_QPA_PLATFORM=offscreen

echo "Running live RDP e2e against sfreerdp-server fixture (127.0.0.1:3390)…"
"$BUILD/tests/test_rdp_live"
