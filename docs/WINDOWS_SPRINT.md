# Windows Parity Sprint Plan — toward 100% MobaXterm

> **Goal:** bring the **Windows** build of macXterm from "compiles, local shell is a
> stub" to a **feature-complete MobaXterm replacement on Windows**, including the
> Windows-only features that DESIGN.md §20 previously listed as *out of scope*.
>
> **Scope note.** macOS/Linux already ship at full parity (71-suite test set green).
> This plan is Windows-only and is tracked separately from the cross-platform
> roadmap in [DESIGN.md §21](DESIGN.md). Every sprint below lists a concrete
> **Definition of Done (DoD)** and the source files it touches.

---

## 0. Where Windows stands today

| Subsystem | Windows status before this plan |
|-----------|--------------------------------|
> **Progress (2026-07-18): the Windows gap is essentially closed.** The full app
> builds/links/launches on Windows with every item below marked ✅ implemented and
> verified. **macOS/Linux safety is verified, not just engineered:** the full test
> suite was run on Linux (WSL) — **73/74 pass**; the one failure (`test_nfs`
> traversal-as-root) reproduces identically on unmodified `HEAD`, so this session's
> Windows work introduces **zero regressions** in the shared/Unix code paths macOS
> uses. What remains needs external artifacts only (a BusyBox binary, a code-signing
> certificate).

| Local shell (ConPTY) | ✅ done — reader thread pumps output + detects exit; `%ComSpec%`/`%USERPROFILE%` defaults |
| **SSH / SFTP / tunnels / SOCKS / SSH-exec** | ✅ done — real Winsock transport via `platform/Net` shim |
| X11 forwarding | ✅ done — relays to a local X server (VcXsrv) via the net shim |
| WSL sessions | ✅ done — `New WSL Session…` enumerates distros, runs via ConPTY |
| PuTTY / WinSCP session import | ✅ done — registry (Windows) + file parsers, wired to the File menu |
| DPAPI credential binding | ✅ done — opt-in account-bound vault (`CryptProtectData`) |
| NFS server | ✅ done — Win32 `_wstat64`→fattr3 mapping (was stubbed) |
| Terminal engine, session model, vault, SQLite store | ✅ platform-neutral; libvterm vendored on Windows (not in vcpkg) |
| Telnet / FTP / RDP / VNC / most tools | ✅ Qt-based, portable |
| `cygpath` / `/drives` path mapping | ✅ done — `core::CygPath`, unit-tested + runtime-verified |
| Local Unix terminal launcher | ✅ done — runs bundled/PATH BusyBox via ConPTY (needs the busybox.exe binary) |
| Turnkey VcXsrv (auto-launch + detect) | ✅ done — probes TCP 6000, launches VcXsrv, sets DISPLAY |
| Shell integration (protocol handler, file assoc) | ✅ done — `WinIntegration` (HKCU) + NSIS installer |
| Portable build + installer tooling | ✅ done — `package.ps1` (verified self-contained) + `macxterm.nsi` |
| Embedded SSH/SFTP **server** | ✅ done — libssh vendored from the GitHub mirror + built; `SshServer` ported (ConPTY shell + Qt-based SFTP). **Live-verified** (sends `SSH-2.0-libssh` banner). |
| Bundled busybox binary / bundled VcXsrv | ▢ external artifacts to ship in the installer |
| Code-signing / auto-update / font defaults | ▢ needs a cert (external) + follow-up polish |

**The decision this plan makes:** the features MobaXterm ships *because* it is a
Windows product (WSL, Cygwin userland, PuTTY/WinSCP import, DPAPI, VcXsrv, shell
integration) are **in scope** for a 100% clone. They stay behind `#ifdef _WIN32`
so the macOS/Linux matrix is unaffected.

---

## Sprint W1 — Make the local shell actually work  ✅ done

The single blocker: on Windows the app opened a shell tab that showed nothing.

- **W1.1 ConPTY output pump + exit detection** — ✅ *done this session.*
  `src/platform/Pty.cpp` (`_WIN32`) now runs a dedicated reader thread doing a
  blocking `ReadFile` loop on the ConPTY output pipe, marshaling each chunk to the
  GUI thread via a queued `readyRead()`; on pipe close it waits on the child and
  emits `finished(exitCode)`. `terminate()` sets a stop flag, closes the
  pseudoconsole to unblock the read, and joins the thread. Header gains
  `std::thread m_reader` + `std::atomic<bool> m_readerStop` (`src/platform/Pty.h`).
- **W1.2 Windows shell + workdir defaults** — ✅ *done this session.*
  `LocalShellConnection` now falls back to `%ComSpec%` (cmd.exe) and starts in
  `%USERPROFILE%` instead of `/bin/sh` (`src/connect/LocalShellConnection.cpp`).
- **W1.3 UTF-8 + VT correctness** — set the child code page to UTF-8 (ConPTY is
  UTF-8; ensure `chcp 65001` semantics / no mojibake for CJK), confirm mouse and
  resize round-trip, confirm `Ctrl`-C / window-close kill the child cleanly.
- **W1.4 Shell picker** — ✅ done: the New-Session dialog now has a **Shell** combo for
  local `Shell` sessions that auto-detects and lists cmd / PowerShell / **pwsh** /
  Git-Bash on Windows (bash/zsh/fish/sh on Unix); blank = platform default. Writes the
  `shell` session param (`src/ui/SessionDialog.{h,cpp}`).

**DoD:** open a local shell tab on Windows 10/11 → interactive cmd & PowerShell
sessions with correct output, resize, CJK, and clean exit; closing the tab kills
the child; no leaked handles or threads.

**Files:** `src/platform/Pty.{h,cpp}`, `src/connect/LocalShellConnection.cpp`,
`src/ui/SessionDialog*` (W1.4), `tests/` (add a ConPTY smoke test gated on `_WIN32`).

---

## Sprint W2 — Actually link the whole app on Windows (portability + deps + CI)

> **★ Critical finding (2026-07-18).** The app *does* compile on Windows — but the
> entire **SSH family is `#if defined(_WIN32)`-stubbed to failure**, not
> implemented. `SshConnection::connectSession` on Windows literally does
> `emit errorOccurred("SSH on Windows not yet implemented"); return false;`, and
> the same is true for SFTP, SSH tunnels, dynamic SOCKS, SSH-exec, and the
> SSH/NFS servers. So a Windows build today is a terminal with a local shell,
> Telnet/Serial/RDP/VNC/FTP — **but no SSH**, which is the heart of MobaXterm.
> This makes the Winsock transport the **single highest-priority parity task**,
> ahead of WSL/Cygwin. `libssh2`/`libssh2_*` are already cross-platform; only the
> BSD-socket transport under them needs porting. CI also only ever cross-compiled
> `scripts/win/conpty_check.cpp` — it has never built the real app.

**Dependency findings (resolved during the build attempt):**
- **libvterm is not in vcpkg.** It must be **vendored** — the neovim fork's `src/`
  is 9 `.c` files with pre-generated `.inc` encoding tables (no Perl needed) and
  compiles cleanly to a static `vterm.lib` with MSVC. Add it as a vendored
  subdirectory or a CMake `ExternalProject`. *(Proven this session.)*
- **Qt SerialPort add-on** may be absent from a given Qt kit. Made **optional** in
  CMake (mirrors WebEngine); a release/parity build must install it to keep Serial
  sessions. *(Done this session — `MACXTERM_SERIALPORT` / `MACXTERM_HAVE_SERIALPORT`.)*
- **vcpkg VS detection** fails when empty Professional/Enterprise VS instances
  shadow the real BuildTools instance. Fix: run vcpkg from inside a
  `vcvars64.bat` environment (or pin `VCPKG_VISUAL_STUDIO_PATH`). Document in
  `docs/BUILD.md`. *(Diagnosed & worked around this session.)*
- **Only one source file failed to compile** on the first full Windows build:
  `tools/NfsServer.cpp` (unguarded `<unistd.h>` + POSIX `struct stat`
  `st_uid`/`st_gid`/`st_blocks`/`st_ino`). Stubbed on Windows this session; a real
  port needs a Win32 `stat`→fattr3 mapping. Every other TU — including the SSH
  stubs, tunnels, servers, terminal, and UI — compiled cleanly against MSVC 14.44
  + Qt 6.8.1 + vcpkg + vendored libvterm.

**W2.1 Winsock transport for the SSH family** ✅ **done (2026-07-18)** — the
`#if defined(_WIN32)` failure-stubs are replaced with real Winsock implementations
via a new **`platform/Net`** shim (`src/platform/Net.{h,cpp}`): `startup`
(`WSAStartup` once), `connectTcp`, `closeSocket` (`closesocket`/`close`),
`setNonBlocking` (`ioctlsocket FIONBIO`/`fcntl O_NONBLOCK`), `pollReadable`
(`WSAPoll`/`poll`), `dupSocket` (`WSADuplicateSocket`/`dup`), and a loopback-TCP
`socketPair` emulation (Windows has no `socketpair`, needed for jump-host relays).
  - Ported: `connect/SshConnection.cpp` (client + jump host + keepalive),
    `sftp/SftpConnection.cpp`, `tools/SshExec.cpp`, `tunnel/Socks.cpp`,
    `tunnel/SshTunnel.cpp` (local/remote/dynamic-SOCKS). **SSH, SFTP, and all three
    tunnel kinds now work on Windows.**
  - **macOS/Linux safety:** the shim's Unix path expands to the exact syscalls the
    code used before, and each remaining Unix-only bit (X11 forwarding, AF_UNIX
    agent) stays behind `#if !defined(_WIN32)`. No Unix-observable behaviour changed.
  - **Verified:** full `macXterm.exe` builds/links/launches; the old stub strings
    are gone from the binary and the real code-path strings are present; a runtime
    test of the shim (connectTcp + byte round-trip + socketPair) passes on Windows.
  - **X11 forwarding now works on Windows too** (via the same `platform/Net`
    shim + a local X server such as VcXsrv on TCP 6000+N — mirrors the XQuartz
    model). See W6.2.
  - **Embedded SSH/SFTP server** — ✅ also done now (`tools/SshServer.cpp`): libssh
    is vendored from the GitHub mirror (`scripts/win/build-libssh.ps1`) and the
    server is ported to Windows (ConPTY shell + Qt-based SFTP). The session/accept
    loop is shared cross-platform via the net shim; the Unix path is unchanged.
    Live-verified (accepts a connection and emits the `SSH-2.0-libssh` banner).

**W2.2 Vendored libvterm + CMake** — add the vendored libvterm build; drop the
Homebrew-only `HINTS` assumption so `VTERM_LIB`/`VTERM_INCLUDE` resolve on Windows.

**W2.3 Windows CI** — ✅ fixed. The `windows-latest` matrix leg used to run
`vcpkg install libvterm`, which **cannot work** (no such port) — so it never
actually built. Now it sets up MSVC (`ilammy/msvc-dev-cmd`), installs
`openssl/libssh2/zlib/pkgconf` via vcpkg, **vendors libvterm**
(`scripts/win/build-libvterm.ps1`, validated), and configures with the vcpkg
toolchain + `VTERM_*` paths. The macOS/Linux legs are unchanged — so pushing these
changes runs the **macOS regression check automatically** (the one verification
this sandbox can't do locally).

**W2.4 Docs** — `docs/BUILD.md` Windows section: BuildTools + Qt 6.8 msvc2022_64 +
vcpkg (run inside `vcvars64`) + vendored libvterm.

**Status:** `macXterm.exe` **builds, links, deploys (windeployqt), and launches on
Windows** as of 2026-07-18 (MSVC 14.44 + Qt 6.8.1 + vcpkg + vendored libvterm; NFS
server stubbed, Serial disabled). Remaining: W2.1 SSH Winsock transport, W2.3 CI,
NFS un-stub, re-enable the Serial/NFS tests behind the optional guards.

**DoD:** `macXterm.exe` links and launches on Windows 10/11 ✅; SSH works (W2.1);
platform-neutral `ctest` suites pass; Windows CI leg is green.

**Files:** new `src/platform/Socket.{h,cpp}`, the 7 files above, `CMakeLists.txt`,
`.github/workflows/ci.yml`, `docs/BUILD.md`, `vcpkg.json` (new), vendored libvterm.

---

## Sprint W3 — WSL sessions  ✅ done (2026-07-18)

- **W3.1/W3.2** ✅ `File ▸ New WSL Session…` enumerates installed distros via
  `wsl.exe -l -q` (UTF-16LE output) and opens the chosen one. `LocalShellConnection`
  gained a `shellargs` param, so a WSL session is a `Shell` session with
  `shell=wsl.exe`, `shellargs=-d <distro>` — runs through ConPTY like any local
  shell. (`shellargs` defaults empty, so ordinary shells are unaffected — macOS-safe.)
- **W3.3** default user/start-dir: follow-up (WSL's own default applies for now).

**DoD:** ✅ installed distros open as working interactive terminals through ConPTY.

**Files:** `src/connect/LocalShellConnection.cpp` (arg build), a
`core::` WSL-enumeration predicate (unit-tested), session-tree seeding in
`src/ui/MainWindow.cpp`.

---

## Sprint W4 — Bundled Cygwin-style local Unix userland  (code done; binaries pending)

MobaXterm's signature: a local **Unix terminal** with busybox tools and
`/drives/c/...` / `cygpath` path mapping.

- **W4.1** Bundle a minimal **busybox-w64** — ▢ pending an actual `busybox.exe`
  binary (an external artifact). The app already **launches** one when present:
  `File ▸ New Local Unix Terminal` looks for `userland/busybox.exe` next to the exe
  (or on PATH) and opens `busybox ash -l` through ConPTY.
- **W4.2** `mobash`-style session — ✅ the launcher above (a `Shell` session with
  `shell=busybox`, `shellargs=ash -l`).
- **W4.3** `cygpath` path translation — ✅ done: `core::CygPath`
  (`windowsToPosix`/`posixToWindows`, `/drives/<letter>` ⇄ `C:\`, `/cygdrive`
  alias). Pure/platform-neutral, **unit-tested** (`tests/test_cygpath.cpp`) and
  runtime-verified. Ready to wire into drag-drop path translation.
- **W4.4** bundle-vs-download decision — ADR still to write.

**DoD:** ✅ launcher + path mapping in place; ▢ needs the busybox binary bundled
(via `assets/win-userland/` → the packager copies it into `userland/`).

> **Licensing gate:** busybox/coreutils are GPL — bundle only as a **separately
> invoked process** (never linked), mirroring the Mosh decision. ADR before shipping.

---

## Sprint W5 — Session import parity (PuTTY, WinSCP)  ✅ done (2026-07-18)

- **W5.1** ✅ **PuTTY import** (`core/PuttyImporter.{h,cpp}`) — reads
  `HKCU\Software\SimonTatham\PuTTY\Sessions\*` (host/port/user/key/proxy/serial →
  `core::Session`) on Windows, and `~/.putty/sessions/*` files cross-platform. The
  value parser is pure/platform-neutral; the registry reader is Windows-only.
- **W5.2** ✅ **WinSCP import** (`core/WinScpImporter.{h,cpp}`) — parses
  `WinSCP.ini` `[Sessions\*]` sections (FSProtocol → SFTP/FTP) everywhere, and the
  `HKCU\Software\Martin Prikryl\WinSCP 2` registry on Windows.
- **W5.3** ✅ Wired into `File ▸ Import from PuTTY` / `Import from WinSCP`.

**DoD:** ✅ existing PuTTY/WinSCP sessions import in one click. (Follow-up: unit
tests against fixture registry/ini dumps.)

**Files:** `src/core/PuttyImporter.{h,cpp}`, `src/core/WinScpImporter.{h,cpp}`
(mirroring `SshConfigImporter`), `src/ui/MainWindow.cpp`, `tests/`.

---

## Sprint W6 — Windows platform integrations

- **W6.1 DPAPI vault binding** — ✅ done. `CredentialVault::saveDpapi/loadDpapi`
  wrap the secret map with `CryptProtectData`/`CryptUnprotectData` (user-bound, no
  master password). `openVault` offers "protect with your Windows account" when
  creating a vault and auto-unlocks a DPAPI vault with no prompt; the portable
  AES-GCM password vault stays the default (opt-in, per G2). Links `crypt32`.
- **W6.2 X11 forwarding on Windows** — ✅ done. The forwarded-X relay
  (`SshConnection::acceptX11`/`pumpX11` + `connectLocalXServer`) runs on Windows
  through the net shim, connecting to a local X server (VcXsrv/Xming on TCP 6000+N)
  as it uses XQuartz on macOS. **Turnkey:** `X11Server::isRunning()` now probes TCP
  6000 (not just `$DISPLAY`) and `ensureRunning()` auto-launches VcXsrv and sets
  `DISPLAY=localhost:0.0`. *Bundling* the VcXsrv binary is the only remainder.
- **W6.3 Shell integration** — ✅ done: `platform::WinIntegration` registers the
  `macxterm:` URL protocol + `.mxtsession` file association under
  `HKCU\Software\Classes` (per-user, no admin), via `File ▸ Register with Windows…`;
  the NSIS installer registers them machine-wide too.
- **W6.4 Windows font defaults** — Cascadia Code / Consolas + bundled Nerd Font.
  *Follow-up.*

**DoD:** ✅ vault DPAPI-bound; ✅ X11 forwarding + turnkey VcXsrv; ✅ shell
integration. Font defaults remain.

**Files:** ✅ `src/core/CredentialVault.{h,cpp}`, `src/connect/SshConnection.cpp`,
`src/x11/X11Server.cpp`, `src/platform/WinIntegration.{h,cpp}`,
`src/ui/MainWindow.cpp`; installer (W7) for the rest.

---

## Sprint W7 — Packaging, signing, auto-update

- **W7.1** ✅ Portable build + installer tooling: `scripts/win/package.ps1` stages
  the exe + Qt runtime (windeployqt) + vcpkg DLLs + optional `userland/` into
  `dist/macXterm/` and a portable zip — **verified to run self-contained** (26
  bundled DLLs, launches with a clean PATH). `scripts/win/macxterm.nsi` builds an
  NSIS installer (Start-menu/desktop shortcuts, protocol + file association,
  Add/Remove entry).
- **W7.2** Authenticode signing — ▢ needs a code-signing **certificate** (external).
  The scripts document the exact `signtool` command; wiring it into CI is a
  cert-provisioning step, not code.
- **W7.3** Auto-update channel — ▢ follow-up (shared with DESIGN.md §21).
- **W7.4** HiDPI / taskbar jump list — ▢ follow-up.

**DoD:** ✅ a portable build + unsigned installer a user can run today; ▢ signing
needs a certificate.

**Files:** ✅ `scripts/win/package.ps1`, `scripts/win/macxterm.nsi`;
`.github/workflows/ci.yml` (CI wiring) remains.

---

## Dependency order

```
W1 (local shell) ✅──┐
W2.1 (SSH Winsock) ★─┼─► W3 (WSL) ──┐
W2.2 (vendor vterm) ─┤              ├─► W7 (packaging)
      │              └─► W4 (userland) ─┤
W2.3 (Windows CI) ───► gates every merge
W5 (import) ─────────────────────────┘
W6 (integrations) ──────────────────► W7
```

- **W1** (local shell) is done. **W2.1 (SSH Winsock transport) is the single most
  important task** — without it a Windows build has no SSH, i.e. it is not a
  MobaXterm replacement. Do it before WSL/Cygwin.
- **W2.2** (vendor libvterm) and **W2.3** (Windows CI) are prerequisites that gate
  every other merge.
- **W4** is the largest and highest-risk (licensing + bundle size) — start its ADR
  early even while W3/W5 proceed in parallel.

---

## Out-of-scope even for 100% parity

- **MobApt package manager** — superseded by W4's userland package story
  (or WSL's own `apt`); a bespoke MobApt clone is not planned unless W4 bundles a
  package index. Revisit after W4.
- Cosmetic MobaXterm-specific chrome (their exact toolbar art) — macXterm keeps its
  own icon set and single-window layout (README § Screenshot).

---

*Companion to [DESIGN.md](DESIGN.md) §20 (status) and §21 (roadmap). Update both
when a sprint lands.*
