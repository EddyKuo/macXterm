#!/usr/bin/env bash
# Deploys a throwaway SSH server fixture (non-privileged, high port, ephemeral
# keys) and runs the guarded-live SSH e2e test against it. This turns
# test_ssh_live from "SKIP (no endpoint)" into a real end-to-end run without any
# external server — verified passing on macOS with the system sshd.
#
# Usage: scripts/live-sshd.sh [path-to-build-dir]   (default: ./build)
set -euo pipefail

BUILD="${1:-build}"
FX="$(mktemp -d)"
trap 'kill "$(cat "$FX/sshd.pid" 2>/dev/null)" 2>/dev/null || true; rm -rf "$FX"' EXIT

ssh-keygen -t ed25519 -f "$FX/hostkey"   -N "" -q
ssh-keygen -t ed25519 -f "$FX/clientkey" -N "" -q
cp "$FX/clientkey.pub" "$FX/authorized_keys"
chmod 600 "$FX/authorized_keys" "$FX/clientkey" "$FX/hostkey"

cat > "$FX/sshd_config" <<EOF
Port 2222
ListenAddress 127.0.0.1
HostKey $FX/hostkey
PidFile $FX/sshd.pid
AuthorizedKeysFile $FX/authorized_keys
PasswordAuthentication no
PubkeyAuthentication yes
UsePAM no
StrictModes no
Subsystem sftp internal-sftp
EOF

SSHD_BIN="$(command -v sshd || echo /usr/sbin/sshd)"
"$SSHD_BIN" -f "$FX/sshd_config" -D -e > "$FX/sshd.log" 2>&1 &
sleep 1

export MACXTERM_SSH_TEST_HOST=127.0.0.1
export MACXTERM_SSH_TEST_PORT=2222
export MACXTERM_SSH_TEST_USER="$(whoami)"
export MACXTERM_SSH_TEST_KEY="$FX/clientkey"
export QT_QPA_PLATFORM=offscreen

echo "Running live SSH e2e against local fixture (127.0.0.1:2222)…"
"$BUILD/tests/test_ssh_live"
