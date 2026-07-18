**English** | [中文](BUILD.zh.md)

# Building macXterm

macXterm is a single-process, native Qt + C/C++ desktop application. It builds
into one executable (`macXterm`) plus a `macxterm_core` static library that all
tests link against. The build is driven by **CMake (≥ 3.21)**.

## Dependencies

All required dependencies are permissively licensed (MIT-compatible).

| Dependency | Purpose | License | Required |
|------------|---------|---------|----------|
| Qt 6 (Core, Gui, Widgets, Network, SerialPort, Sql, Test) | UI, event loop, sockets, serial, SQLite, tests | LGPLv3 (**dynamic link only**) | ✅ |
| OpenSSL 3 | AES-256-GCM + Argon2id/scrypt (credential vault) | Apache-2.0 | ✅ |
| libvterm | VT100/VT220/xterm terminal emulation | MIT | ✅ |
| libssh2 | SSH / SFTP / tunnels | BSD-3 | ✅ |
| zlib | VNC ZRLE inflate | zlib | ✅ (usually system-provided) |
| FreeRDP 3 | real RDP protocol (autodetected) | Apache-2.0 | optional |
| libpcap | packet capture (autodetected) | BSD | optional |

> **Notes.** Argon2id comes from OpenSSL 3.2+'s `EVP_KDF` — there is **no**
> separate `argon2` dependency, and older OpenSSL (3.0/3.1, e.g. Ubuntu 24.04)
> falls back to scrypt automatically. Qt is linked **dynamically** to satisfy
> LGPL under an MIT project — do **not** static-link Qt without a commercial
> license. If FreeRDP is found, RDP is built with real protocol support
> (`MACXTERM_HAVE_FREERDP`); otherwise RDP builds as a scaffold.

### macOS (Homebrew)

```sh
brew install qt openssl@3 libvterm libssh2 zlib cmake
brew install freerdp        # optional — enables real RDP
```
(zlib ships with macOS; the Homebrew formula is only needed if CMake can't find
the system one.)

### Linux (Debian / Ubuntu)

```sh
sudo apt install qt6-base-dev libqt6serialport6-dev libssl-dev \
                 libvterm-dev libssh2-1-dev zlib1g-dev cmake ninja-build
sudo apt install libfreerdp-dev libpcap-dev   # optional — real RDP / packet capture
```

### Windows

**Validated build (MSVC 2022 + Qt 6.8 + vcpkg).** This recipe builds, links, and
launches `macXterm.exe` today; see [WINDOWS_SPRINT.md](WINDOWS_SPRINT.md) for the
feature-parity roadmap (SSH is still stubbed on Windows — top-priority W2.1).

1. **Toolchain:** Visual Studio 2022 **Build Tools** with the *Desktop C++*
   workload (MSVC 14.4x + Windows 10/11 SDK), CMake, and Ninja.
2. **Qt:** install Qt 6.8 `msvc2022_64` (the SerialPort add-on is optional — the
   build disables the Serial session when it is absent).
3. **vcpkg deps:** `openssl`, `libssh2[core,openssl,zlib]`, `zlib`, `pkgconf`
   (triplet `x64-windows`). Run vcpkg **from inside a `vcvars64.bat` shell** so it
   uses the Build Tools instance — empty `Professional`/`Enterprise` shells confuse
   its auto-detection.
4. **libvterm is NOT in vcpkg** — vendor it. The neovim fork's `src/*.c` (9 files,
   with pre-generated `.inc` encoding tables) compiles to a static `vterm.lib` with
   MSVC directly; pass its paths via `-DVTERM_LIB=` / `-DVTERM_INCLUDE=`.
5. **libssh (server-capable) is optional** — enables the embedded SSH/SFTP server. Its
   vcpkg port downloads from `libssh.org`, which is not always reachable; vendor it from
   the GitHub mirror instead with `scripts/win/build-libssh.ps1` and pass
   `-DSSH_LIB=` / `-DSSH_INCLUDE=`. Omit these to build without the embedded server.

```bat
:: from an x64 Native Tools (vcvars64) prompt, with Ninja on PATH
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows ^
  -DCMAKE_PREFIX_PATH=C:/Qt/6.8.1/msvc2022_64 ^
  -DVTERM_LIB=<vendored>/vterm.lib -DVTERM_INCLUDE=<vendored>/include ^
  -DSSH_LIB=<vendored>/ssh.lib "-DSSH_INCLUDE=<libssh>/include;<libssh>/_build/include"
cmake --build build --parallel
windeployqt --release build/src/macXterm.exe   :: stage the Qt DLLs to run it
```

ConPTY (`src/platform/Pty.cpp`) requires Windows 10 (RS5) SDK headers, which the
source configures automatically. The local shell defaults to `%ComSpec%`
(cmd.exe) starting in `%USERPROFILE%`.

## Configure, build, test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
( cd build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure )
```

`QT_QPA_PLATFORM=offscreen` lets the GUI-linked tests run headlessly (CI and
servers). The suite is expected to be **100% green** on macOS and Linux.

Convenience wrappers:

```sh
scripts/build.sh          # configure + build + test (Unix)
scripts/build.ps1         # same, Windows PowerShell
scripts/linux-build.sh    # build + test inside an Ubuntu container (needs Docker)
```

## CMake options

| Option | Default | Meaning |
|--------|---------|---------|
| `MACXTERM_BUILD_GUI` | ON | Build the `macXterm` GUI executable |
| `MACXTERM_BUILD_TESTS` | ON | Build the CTest suite |
| `MACXTERM_HAVE_FREERDP` | auto | Defined when FreeRDP is detected → real RDP |

## Live end-to-end fixtures (optional)

Some protocol tests are "guarded-live": they run against a real server when one
is configured, and `QSKIP` otherwise. Helper scripts deploy throwaway fixtures:

```sh
scripts/live-sshd.sh      # deploy a local sshd + run the live SSH e2e test
scripts/rdp-fixture.sh    # deploy sfreerdp-server + run the live RDP e2e test
```

## Run the app

```sh
./build/src/macXterm
```

The main window opens with a session-tree sidebar, a tabbed terminal area, a
toolbar (New Shell / New Session / MultiExec / Tunnel / Settings / Vault), and an
initial local-shell tab.

## Project layout

```
src/
  core/       Session, SessionTree, Store (SQLite), CredentialVault, Settings,
              Macro, ShortcutRegistry, CliOptions, SshConfigImporter, ...
  platform/   Pty (PAL: forkpty / ConPTY)
  term/       ScreenBuffer, VtEngine (libvterm), ColorScheme
  connect/    IConnection + SSH/Telnet/Serial/Mosh/RSH/Rlogin/XDMCP/RDP/VNC
  sftp/       RemotePath, SftpEntry, SftpConnection
  tunnel/     Tunnel, TunnelManager, LocalForwarder
  tools/      PortScanner, TFTP/FTP/HTTP servers, TextDiff, RemoteMonitor, HostKey
  x11/        X11Display
  ui/         MainWindow, TerminalWidget, Session/Tunnel/Settings/Vault dialogs
tests/        Qt Test suites wired into CTest
docs/         User & developer documentation
scripts/      build + live-fixture helpers
```

For the full technical design, see [DESIGN.md](DESIGN.md).
