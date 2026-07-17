<h1 align="center">macXterm</h1>

<p align="center">
  <b>A native, cross-platform, MIT-licensed clone of <a href="https://mobaxterm.mobatek.net/">MobaXterm</a>.</b><br>
  A single-process Qt 6 / C++ desktop app — one codebase, no artificial limits.
</p>

<p align="center"><b>English</b> | <a href="README.zh.md">中文</a></p>

macXterm is an all-in-one remote-computing toolbox: a **tabbed terminal**, **graphical
SFTP/FTP browsers**, **SSH tunnels**, **RDP/VNC** remote desktops, **X11 forwarding**,
an **encrypted credential vault**, and a drawer of **network tools and light servers** —
all in one native application, on **macOS, Linux, and Windows**.

> **Status:** builds clean and the **full 71-suite test set passes** on macOS & Linux
> (incl. live SSH/RDP end-to-end tests against local fixtures). Primary targets are
> macOS and Linux; the Windows code paths compile but the ConPTY local shell is not yet
> complete. See [Feature coverage](#feature-coverage) and [`docs/DESIGN.md`](docs/DESIGN.md) §20.

---

## Highlights

- **14 session types** — SSH, Telnet, RSH, Rlogin, Serial, Mosh, SFTP, FTP, Amazon S3, local Shell, RDP, VNC, XDMCP, and an embedded Browser.
- **Full-featured terminal** — VT100/VT220/xterm via `libvterm`, 256-color **and true-color**, emoji / astral-plane glyphs, **CJK input-method (IME) typing**, syntax highlighting, session logging, bracketed paste, mouse reporting (1000/1002/1003 + SGR 1006), scrollback search, and `Ctrl`/`Cmd`-click to open URLs.
- **Per-session overrides** — font, colour scheme, scrollback, and Backspace code can be set globally *or* per bookmark.
- **Tabs & panes** — drag-reorder, detach/reattach to floating windows, 2/2/4 split panes, and **MultiExec** (broadcast one keystroke to every visible pane).
- **Graphical SFTP & FTP browsers** — drag-and-drop transfer, follow-the-terminal's-folder, double-click **remote edit & auto-resave**, recursive folder transfer with a cancelable progress bar.
- **SSH ecosystem** — jump hosts, agent auth, X11 forwarding, compression, keepalive, run-a-remote-command, and local/remote/**dynamic (SOCKS)** tunnels (RDP/VNC can route through a jump host too).
- **Remote desktops** — **RDP** via FreeRDP (resolution, clipboard/drive/audio redirection, NLA) and **VNC** via a from-scratch MIT RFB client with **Raw / CopyRect / RRE / Hextile / ZRLE** decoding — both fully interactive (mouse + keyboard), with a view-only mode.
- **Organized session tree** — folders + per-bookmark icons, a **live filter box** (name/host/user/folder), a **right-click context menu** (edit/rename/duplicate/move-to-folder/set-icon/copy-SSH-command/delete), an encrypted vault, and imports from OpenSSH `~/.ssh/config` and `MobaXterm.ini`.
- **Built-in light servers** — TFTP, HTTP, FTP, Telnet, CRON, **NFSv3 (read/write)**, and SSH/SFTP — launched from the toolbar, with no runtime cap.
- **Network toolbox** — port scanner, subnet/CIDR sweep, packet capture (`libpcap`), SSH key generation, image viewer, text & folder diff, and a live remote CPU/RAM/NET monitor bar.

## Screenshot / icon

macXterm ships with a friendly terminal-mascot app icon
([`assets/icon/`](assets/icon/)). The whole UI — including the menu bar on macOS —
lives inside the main window, matching MobaXterm's single-window layout.

## Quick start

macXterm builds with CMake against Qt 6, OpenSSL 3, `libvterm`, `libssh2`, and `zlib`
(FreeRDP is optional and enables real RDP). Full per-platform instructions are in
**[`docs/BUILD.md`](docs/BUILD.md)**.

```sh
# macOS (Homebrew)
brew install qt openssl@3 libvterm libssh2 zlib cmake
brew install freerdp          # optional — enables real RDP

# configure, build, test
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure

# run
open ./build/src/macXterm.app        # macOS
./build/src/macXterm                 # Linux
```

## Documentation

| Document | Audience |
|----------|----------|
| **[docs/USER_GUIDE.md](docs/USER_GUIDE.md)** | Using macXterm — sessions, terminal, SFTP/FTP, tunnels, vault, tools |
| **[docs/BUILD.md](docs/BUILD.md)** | Building from source on macOS / Linux / Windows |
| **[docs/DESIGN.md](docs/DESIGN.md)** | Technical design — architecture, modules, protocols, data model, threading, security |

## Feature coverage

macXterm covers MobaXterm's **non-Windows** functional surface. The gaps are, by design:

- **Windows-only features** (no macOS/Linux equivalent, or already covered otherwise):
  WSL sessions (use the local Shell), the Cygwin `/drives`·`/registry`·`cygpath` shell
  extensions and the MobApt package manager (macOS/Linux are already Unix — use the
  system tools / Homebrew), PuTTY-registry & WinSCP import, and the Windows shell/protocol
  handlers. See [`docs/DESIGN.md`](docs/DESIGN.md) §20.
- **Needs external infrastructure to finish**: XDMCP's post-handshake X-display redirection
  (needs a real display manager) and a *bundled* X.Org server (macXterm launches the
  platform's own X server — XQuartz / VcXsrv — instead).

## Project layout

```
src/
  core/       Session model, SQLite store, credential vault, settings, macros, CLI, importers
  platform/   PAL — pseudo-terminal (forkpty / ConPTY)
  term/       VT engine (libvterm), screen buffer, colour schemes
  connect/    IConnection abstraction + SSH/Telnet/Serial/Mosh/RSH/Rlogin/XDMCP/RDP/VNC/FTP
  sftp/       SFTP & FTP browser back-ends + remote-path/transfer helpers
  tunnel/     SSH tunnels (local/remote/dynamic) + TCP forwarder
  tools/      port scanner, packet capture, TFTP/HTTP/FTP/Telnet/CRON/NFS/SSH servers, diff, monitor
  x11/        X11 forwarding / DISPLAY management
  ui/         main window, terminal widget, session/tunnel/settings/vault dialogs, SFTP panel
tests/        Qt Test suites wired into CTest (71 suites)
docs/         user & developer documentation
assets/icon/  app icon (SVG master + .icns)
scripts/      build, live-fixture, and GitHub-push helper scripts
```

## Technology & licensing

macXterm is **MIT-licensed** and links only permissively-licensed dependencies: Qt 6
(LGPL, dynamically linked), OpenSSL (Apache-2.0), `libvterm` (MIT), `libssh2` (BSD),
`zlib`, and FreeRDP (Apache-2.0). VNC is a from-scratch RFB implementation and Mosh is
invoked as a separate process, so **no GPL code is linked**. See
[`docs/DESIGN.md`](docs/DESIGN.md) §18 (Licensing) and §19 (design decisions).

## Contributing

macXterm targets three platforms from one codebase; changes should keep the macOS and
Linux test matrix green (`ctest --test-dir build`). Non-GUI logic lives in the
`macxterm_core` library and is unit-tested; platform differences hide behind the thin
abstraction layer in `src/platform/`. See [`docs/DESIGN.md`](docs/DESIGN.md) for the
architecture and conventions.

## License

MIT — see [LICENSE](LICENSE).
