#!/usr/bin/env bash
# LLVM source-based code coverage for macxterm_core (the unit-tested library).
# Usage: scripts/coverage.sh [report|show <file>]
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT=$(pwd)
BUILD=build-cov
PROF=$BUILD/prof

cmake -S . -B "$BUILD" -DMACXTERM_COVERAGE=ON -DMACXTERM_BUILD_GUI=OFF >/dev/null
cmake --build "$BUILD" -j4 >/dev/null

rm -rf "$PROF"; mkdir -p "$PROF"

# ── Spin up a throwaway local sshd so the guarded-live SSH tests run (covers
#    SshConnection / SftpConnection / SshTunnel / SshExec) instead of skipping. ──
FX=$(mktemp -d)
cleanup_sshd() { kill "$(cat "$FX/sshd.pid" 2>/dev/null)" 2>/dev/null || true; rm -rf "$FX"; }
trap cleanup_sshd EXIT
if command -v ssh-keygen >/dev/null && { command -v sshd >/dev/null || [ -x /usr/sbin/sshd ]; }; then
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
AllowTcpForwarding yes
PermitTTY yes
Subsystem sftp internal-sftp
EOF
  SSHD_BIN="$(command -v sshd || echo /usr/sbin/sshd)"
  "$SSHD_BIN" -f "$FX/sshd_config" -D -e > "$FX/sshd.log" 2>&1 &
  sleep 1
  export MACXTERM_SSH_TEST_HOST=127.0.0.1 MACXTERM_SSH_TEST_PORT=2222
  export MACXTERM_SSH_TEST_USER="$(whoami)" MACXTERM_SSH_TEST_KEY="$FX/clientkey"
  export QT_QPA_PLATFORM=offscreen
  echo "sshd fixture up on 127.0.0.1:2222"
else
  echo "sshd/ssh-keygen not found — live SSH tests will skip"
fi

# Collect the actual Mach-O test executables (skip .dSYM bundles / directories).
BINS=()
for t in "$BUILD"/tests/test_*; do
  case "$t" in *.dSYM) continue;; esac
  [ -f "$t" ] && [ -x "$t" ] && BINS+=("$t")
done

# Run each test binary. The %p (pid) pattern gives every process — including any
# child helper it spawns (e.g. sshserver_fixture) — its own raw profile so their
# coverage is captured too.
for t in "${BINS[@]}"; do
  LLVM_PROFILE_FILE="$PROF/$(basename "$t")-%p.profraw" "$t" >/dev/null 2>&1 || true
done
echo "ran ${#BINS[@]} test binaries"

xcrun llvm-profdata merge -sparse "$PROF"/*.profraw -o "$PROF/merged.profdata"

# llvm-cov wants one binary as the positional "main" object, the rest via -object.
MAIN="${BINS[0]}"
OBJS=()
for t in "${BINS[@]:1}"; do OBJS+=("-object" "$t"); done
# Include helper binaries that also ran under coverage (e.g. the SSH fixture).
[ -x "$BUILD/tests/sshserver_fixture" ] && OBJS+=("-object" "$BUILD/tests/sshserver_fixture")

MODE="${1:-report}"
if [ "$MODE" = "show" ]; then
  xcrun llvm-cov show "$MAIN" "${OBJS[@]}" -instr-profile="$PROF/merged.profdata" \
    "$ROOT/src/${2:-}"
else
  xcrun llvm-cov report "$MAIN" "${OBJS[@]}" -instr-profile="$PROF/merged.profdata" \
    -ignore-filename-regex='(tests/|_autogen/|moc_|/usr/|/opt/)' 2>/dev/null | tail -85
fi
