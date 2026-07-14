# macXterm

**A native, cross-platform, MIT-licensed clone of [MobaXterm](https://mobaxterm.mobatek.net/).**
Built with Qt 6 and C/C++ as a single-process desktop application — one codebase for
**Windows, macOS, and Linux**, with no artificial limits.

macXterm is an all-in-one remote-computing toolbox: a tabbed terminal, a graphical
SFTP browser, SSH tunnels, an X11-forwarding integration, and a set of network tools —
in one native application.

---

## Highlights

- **Tabbed terminal** with VT100 / VT220 / xterm emulation (backed by `libvterm`), 256-color, UTF-8, and color schemes (Dark / Light / Solarized).
- **Ten session types**: SSH, Telnet, Serial, Mosh, RSH, Rlogin, XDMCP, RDP, VNC, and a local shell.
- **MultiExec** — broadcast keystrokes to multiple terminal panes at once.
- **Graphical SFTP browser** over the same SSH connection (`libssh2`).
- **SSH tunnels** — local / remote / dynamic (SOCKS) — plus jump hosts.
- **Encrypted credential vault** — AES-256-GCM with an Argon2id (or scrypt) master-password KDF; secrets never touch plaintext or the settings database.
- **SQLite session store** — folders, bookmarks, known-hosts pinning; imports sessions from OpenSSH `~/.ssh/config` (PuTTY/WinSCP import planned).
- **RDP** via FreeRDP and **VNC** via a from-scratch MIT RFB client (no GPL libraries).
- **Built-in tools** — port scanner, TFTP / FTP / HTTP light servers, text diff, remote CPU/RAM monitor, SSH key fingerprinting.
- **X11 forwarding** that integrates the platform's X server (XQuartz / VcXsrv / native).
- **No artificial limits** — unlike MobaXterm Home's caps on sessions, tunnels, macros, and daemon runtime.

## Status

macXterm builds cleanly and its full test suite passes on **macOS and Linux**
(40 test suites, 100% green, including live SSH and RDP end-to-end tests against local
server fixtures). Windows support is implemented in-source (ConPTY pseudo-console,
Winsock transport) and the Windows-specific code cross-compiles to a native PE binary;
the full Windows GUI build is validated by the maintainer on Windows.

Feature depth varies by area — see [`docs/DESIGN.md`](docs/DESIGN.md) §"Implementation Status"
for exactly what is fully working, what needs a live server, and what remains on the roadmap.

## Building

macXterm uses CMake and depends on Qt 6, OpenSSL 3, `libvterm`, and `libssh2`
(FreeRDP is optional and enables real RDP). Full instructions — including per-platform
dependency setup — are in **[`docs/BUILD.md`](docs/BUILD.md)**.

```sh
# macOS (Homebrew)
brew install qt openssl@3 libvterm libssh2 cmake

# configure, build, test
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
( cd build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure )

# run
./build/src/macXterm
```

## Documentation

| Document | Audience |
|----------|----------|
| **[docs/USER_GUIDE.md](docs/USER_GUIDE.md)** | Using macXterm — sessions, terminal, SFTP, tunnels, vault, tools |
| **[docs/BUILD.md](docs/BUILD.md)** | Building from source on Windows / macOS / Linux |
| **[docs/DESIGN.md](docs/DESIGN.md)** | Full technical design — architecture, modules, protocols, data model, threading, security |

## Project layout

```
src/
  core/       Session model, SQLite store, credential vault, settings, macros, CLI, importers
  platform/   PAL — pseudo-terminal (forkpty / ConPTY)
  term/       VT engine (libvterm), screen buffer, color schemes
  connect/    IConnection abstraction + SSH/Telnet/Serial/Mosh/RSH/Rlogin/XDMCP/RDP/VNC
  sftp/       SFTP browser back-end + remote-path helpers
  tunnel/     SSH tunnels (local/remote/dynamic) + TCP forwarder
  tools/      port scanner, TFTP/FTP/HTTP servers, text diff, remote monitor
  x11/        X11 forwarding / DISPLAY management
  ui/         Main window, terminal widget, session/tunnel/settings/vault dialogs
tests/        Qt Test suites wired into CTest
docs/         User & developer documentation
scripts/      Build and live-fixture helper scripts
```

## Technology & licensing

macXterm is **MIT-licensed** and links only permissively-licensed dependencies:
Qt 6 (LGPL, dynamically linked), OpenSSL (Apache-2.0), `libvterm` (MIT), `libssh2`
(BSD), and FreeRDP (Apache-2.0). VNC is a from-scratch RFB implementation and Mosh is
invoked as a separate process, so no GPL code is linked. See [LICENSE](LICENSE) and
[`docs/DESIGN.md`](docs/DESIGN.md) §"Licensing" for details.

## Contributing

macXterm targets three platforms from one codebase; all changes must keep the
macOS and Linux CI matrix green. Platform-specific code lives behind a thin
abstraction layer in `src/platform/`. See [`docs/DESIGN.md`](docs/DESIGN.md) for the
architecture and conventions.

## License

MIT — see [LICENSE](LICENSE).
