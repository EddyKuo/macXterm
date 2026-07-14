# macXterm User Guide

Welcome to **macXterm** — a fast, native, cross-platform terminal and remote-computing
toolbox. If you have ever used MobaXterm on Windows and wished for the same tabbed
terminal, SSH/SFTP, tunnels, and network tools on macOS or Linux (as well as Windows),
macXterm is built to feel familiar while being open, MIT-licensed, and free of artificial
limits.

This guide is for **end users**. It walks through everything you can do from the
application window. For building from source, see [`BUILD.md`](BUILD.md).

---

## 1. What macXterm is and who it's for

macXterm is a single desktop application that brings together the tools a developer,
system administrator, or network engineer reaches for every day:

- A **tabbed terminal** with real VT100/VT220/xterm emulation.
- **Remote sessions** over many protocols: SSH, Telnet, Serial, Mosh, RSH, Rlogin,
  XDMCP, RDP, VNC — plus a plain local **Shell**.
- A graphical **SFTP file browser** riding on your SSH connection.
- **SSH tunnels** (local, remote, dynamic/SOCKS) and jump-host support.
- **MultiExec** — type once, broadcast to many panes at the same time.
- An encrypted **credential vault** protected by a master password.
- A set of built-in **network tools**: port scanner, light TFTP/FTP/HTTP servers,
  text diff, and a remote system monitor.
- **X11 forwarding** that integrates your platform's X server.

It is written in native **Qt + C/C++** and runs on **Windows, macOS, and Linux** from a
single codebase. It is distributed under the **MIT license**. Unlike MobaXterm's free
"Home" edition, macXterm imposes **no caps** on the number of saved sessions, tunnels, or
macros.

---

## 2. Installing and launching

### Installing

macXterm builds into a single executable. If a prebuilt binary is available for your
platform, run it directly. To build it yourself, follow [`BUILD.md`](BUILD.md) — in short,
you need Qt 6, OpenSSL 3.2+, libvterm, and libssh2, then a standard CMake configure/build.

### First launch

When macXterm starts you'll see a three-part window:

- **Session-tree sidebar** (left) — a dockable "Sessions" panel that lists your saved
  connection profiles under a root folder. Double-click (activate) any entry to open it.
- **Tabbed terminal area** (center) — where each open session lives in its own closable,
  movable tab.
- **Toolbar** (top) — the main action bar with these buttons:
  - **New Shell** — open another local shell tab.
  - **New Session** — create/open a remote session via a dialog.
  - **MultiExec** — a toggle that broadcasts your keystrokes to multiple panes.
  - **Tunnel** — open the SSH tunnel dialog.
  - **Settings** — open the preferences dialog.
  - **Vault** — unlock (or set up) your encrypted credential vault.

On startup macXterm automatically opens **one local-shell tab** so you have a working
prompt immediately — no configuration required.

---

## 3. Session types and how to create them

macXterm connects to remote systems through **sessions**. Click **New Session** on the
toolbar to open the Session dialog. It has these fields:

| Field | Meaning |
|-------|---------|
| **Name** | A label for the session; also the tab title and sidebar entry. |
| **Type** | The protocol (see below). |
| **Host** | Hostname or IP address of the remote machine. |
| **Port** | TCP/serial port. Leave at 0 to use the protocol's default. |
| **Username** | Login name (for protocols that authenticate). |

When you click **OK**, the session is validated, saved to the sidebar tree, and opened in
a new tab.

> **Default ports** are filled in automatically when you leave Port empty: SSH/Mosh **22**,
> Telnet **23**, Rlogin **513**, RSH **514**, RDP **3389**, VNC **5900**, XDMCP **177**.

### The supported types

The New Session dialog offers **SSH, Telnet, Serial, Mosh, RDP, VNC, and Shell**. A few
additional protocols (**RSH, Rlogin, XDMCP**) are supported by the engine and are created
by importing or by choosing the matching type where offered.

- **SSH** — the workhorse. Needs **Host**, **Username**, and optionally **Port**.
  Authentication is by password (supplied at connect time, typically from the credential
  vault) or by a private key (`keyfile` parameter, with an optional passphrase). The same
  SSH session also carries SFTP, tunnels, and X11 forwarding.
- **Telnet** — plain Telnet with option negotiation. Needs **Host** (and **Port** if not 23).
- **Serial** — a serial console for hardware. Uses the serial device as the port, with
  line settings (baud, data bits, parity, stop bits, flow control) defaulting to **9600 8N1**.
- **Mosh** — mobile shell, roaming-friendly over SSH bootstrap. Needs **Host**/**Username**.
- **RSH** / **Rlogin** — legacy remote-shell protocols over a simple TCP stream, each with
  its startup handshake. Needs **Host** (and **Username** for the login handshake).
- **XDMCP** — X Display Manager query bootstrap. Needs **Host**.
- **RDP** — Remote Desktop to Windows hosts. Needs **Host** (and **Username**); default port 3389.
- **VNC** — remote framebuffer / screen sharing. Needs **Host**; default port 5900.
- **Shell** — a **local** command shell on your own machine. No host/username needed; this
  is what the startup tab uses and what **New Shell** opens.

Which fields matter varies by type: network protocols (SSH/Telnet/Mosh/RDP/VNC/RSH/Rlogin/
XDMCP) care about **Host** and, where relevant, **Username** and **Port**; **Serial** cares
about the device/port and line settings; **Shell** ignores host/port entirely.

---

## 4. Using the terminal

Each session opens in its own **tab**. Tabs are **closable** (the × on the tab, or
`Ctrl+Shift+W`) and **movable** (drag to reorder). Switch tabs with `Ctrl+Tab` /
`Ctrl+Shift+Tab`.

### Emulation

The terminal is driven by a real **VT100/VT220/xterm** emulator (built on libvterm), so
full-screen curses apps — `vim`, `htop`, `tmux`, `less`, and friends — render correctly,
including colors, cursor movement, and device-status replies.

### Color schemes

Three schemes ship in the box, selectable in **Settings → Terminal → Color scheme**:

- **Dark** (default) — light-grey text on black, standard xterm 16-color ANSI palette.
- **Light** — black text on white.
- **Solarized Dark** — the familiar Solarized background/foreground with tuned base colors.

### Fonts

Set the **Font** family and **Font size** (6–72 pt) in **Settings → Terminal**. You can
also configure the **Scrollback** buffer (number of lines retained, up to 1,000,000).

---

## 5. MultiExec — broadcast typing to multiple panes

**MultiExec** lets you drive several sessions at once: type a command in one place and it
is sent to every participating terminal — ideal for running the same operation across a
fleet of servers.

- Click the **MultiExec** toolbar button to toggle broadcast mode on/off.
- While active, keystrokes are delivered to **every enabled target** pane.
- Individual panes can **opt out** of the broadcast, and panes that have been closed are
  skipped automatically, so you won't accidentally type into a dead session.

MultiExec has **no cap** on how many panes participate.

---

## 6. SFTP file browser

Every SSH session can carry a graphical **SFTP** channel over the same authenticated
connection — no second login. With SFTP you can:

- **Browse** remote directories (entries are listed and sorted for you).
- **Download** files from the remote host to your machine.
- **Upload** files from your machine to the remote host.

Because SFTP shares the SSH transport, it becomes available once your SSH session is
connected and authenticated. Live browsing requires a real remote `sshd`; the underlying
path-handling and entry modelling are independently verified.

---

## 7. SSH tunnels and jump hosts

Click **Tunnel** on the toolbar to open the SSH Tunnel dialog. Choose a **Type** and fill
in the endpoints:

| Type | What it does | Fields used |
|------|--------------|-------------|
| **local** | Listen on a local `bind:bindPort` and forward through SSH to `target:targetPort`. | Bind address, Bind port, Target host, Target port |
| **remote** | Listen on the SSH **server** and forward connections back to a `target:targetPort` reachable from your side. | Bind address, Bind port, Target host, Target port |
| **dynamic** | Run a **SOCKS proxy** on `bind:bindPort` (no fixed target). | Bind address, Bind port |

The dialog defaults the bind address to `127.0.0.1`. A tunnel is valid when it has a bind
port and — for local/remote — a target host and port. The tunnel manager rejects invalid
tunnels and refuses **bind-port collisions**, so two tunnels can't fight over the same
local port. There is **no limit** on the number of tunnels you can define.

### Jump hosts

macXterm understands **jump hosts** (SSH `ProxyJump`). When you import an OpenSSH config
that specifies `ProxyJump`, the jump host is preserved on the session (as its `jumphost`
parameter) so connections are routed through the bastion.

---

## 8. Credential vault and master password

macXterm stores your secrets (such as SSH passwords) in an encrypted **credential vault**,
unlocked by a single **master password**. Click **Vault** on the toolbar:

- **Set Master Password** (first-time / create mode) — enter a master password of **at
  least 8 characters** and confirm it.
- **Unlock Vault** — enter your master password to decrypt stored secrets for the session.

### How your secrets are protected

The vault is strong, modern cryptography — not obfuscation:

- The master password is stretched into a 256-bit key with **Argon2id** (memory-hard KDF,
  64 MiB / 3 iterations). On older systems without Argon2id, it transparently falls back to
  **scrypt**; the vault records which KDF it used so it always decrypts on any build.
- Secrets are encrypted with **AES-256-GCM**, an authenticated cipher — if the file is
  tampered with or the password is wrong, decryption fails cleanly rather than returning
  garbage.
- A random **salt** and **nonce** are generated per save, and the derived key and decrypted
  plaintext are wiped from memory after use.

When an SSH session connects, the vault can inject the stored password so you don't retype
it.

---

## 9. Settings

Open **Settings** from the toolbar. It has two tabs:

### Terminal
- **Font** — font family for the terminal.
- **Font size** — 6 to 72 pt.
- **Color scheme** — Dark, Light, or Solarized Dark.
- **Scrollback** — number of retained lines (0 – 1,000,000).

### X11
- **Auto-start X server** — start the platform X server automatically when needed.
- **Enable SSH X11 forwarding** — tunnel remote GUI apps back to your display over SSH.

---

## 10. Keyboard shortcuts

These are the default bindings. On macOS, `Ctrl` is mapped to `⌘` automatically, so a
single definition works across platforms.

| Action | Shortcut |
|--------|----------|
| New terminal | `Ctrl+Alt+T` |
| Close terminal | `Ctrl+Shift+W` |
| Next tab | `Ctrl+Tab` |
| Previous tab | `Ctrl+Shift+Tab` |
| Detach tab | `Ctrl+Shift+D` |
| Toggle full screen | `F11` |
| Paste | `Shift+Ins` |
| Quick Connect | `Ctrl+Shift+Q` |
| Toggle X11 | `Ctrl+Shift+X` |
| Open editor | `Ctrl+Shift+M` |

Shortcuts can be **rebound**; the registry refuses a binding that would collide with an
already-assigned action.

---

## 11. Importing existing sessions

You don't have to recreate connections you already have.

### OpenSSH (`~/.ssh/config`)

macXterm imports your OpenSSH client config directly. It reads each `Host` block and
creates an SSH session from it, mapping:

- `Host` → session name
- `HostName` → host (falls back to the alias if omitted)
- `User` → username
- `Port` → port
- `IdentityFile` → private-key file (`keyfile`)
- `ProxyJump` → jump host (`jumphost`)

Wildcard patterns (`Host *`, `?`) are skipped, and `=` or space separators are both
handled. Imported sessions land under an **"Imported (ssh_config)"** folder.

### PuTTY

macXterm's session store uses an INI-style format aligned with PuTTY's saved-session
layout. Full PuTTY import is planned and slots into the same importer path used for
OpenSSH config.

---

## 12. Built-in tools

macXterm bundles the small network utilities you'd otherwise open a separate app for. None
of them carry the runtime caps that MobaXterm Home imposes.

- **Port scanner** — TCP-connect scanner. Check a single host/port, or scan an inclusive
  **port range**, receiving each open port as it's found plus a final open-count.
- **TFTP server** — a read-only **TFTP** (RFC 1350) server over UDP that serves files from
  a chosen root directory. It handles read requests (RRQ) and block/ACK; write requests are
  refused.
- **FTP server** — a small **FTP** (RFC 959 subset) control server with anonymous read
  access — handy for quick file serving (`USER`/`PASS`/`SYST`/`PWD`/`TYPE`/`QUIT`).
- **HTTP server** — a minimal **HTTP GET** file server that shares files from a directory.
  Intentionally tiny — great for a quick download, not a general web server.
- **Text diff** — a line-level, LCS-based diff (MobaTextDiff-style) that reports Equal /
  Added / Removed lines between two texts.
- **Remote monitor** — parses remote system stats (CPU from `/proc/stat`, memory from
  `/proc/meminfo`) gathered over SSH into usage numbers for a monitoring status readout.

---

## 13. X11 forwarding

macXterm can display **remote graphical (X11) applications** on your desktop over SSH. It
does **not** ship its own X server; instead it integrates the **platform's X server** and
manages the `DISPLAY` handed to forwarded apps:

- **macOS** — **XQuartz**
- **Windows** — an X server such as **VcXsrv**
- **Linux** — your **native X.Org** server

Enable **SSH X11 forwarding** in **Settings → X11**, optionally with **Auto-start X
server**, then run a GUI program in an SSH session — it appears as a local window.
`Ctrl+Shift+X` toggles X11 quickly.

---

## 14. Current status and limits

macXterm is honest about what needs a live peer versus what works entirely on your machine.

**Fully working locally (no server needed):**
- Local **Shell** tabs and the VT100/xterm terminal emulation
- Color schemes, fonts, scrollback, and Settings
- The **credential vault** (AES-256-GCM + Argon2id/scrypt)
- **MultiExec** broadcast logic
- **Text diff**, **port scanner**, and the light **TFTP/FTP/HTTP** servers
- OpenSSH **config import**
- Tunnel and session **validation** (port-collision and field checks)

**Needs a live remote (or hardware) to exercise end to end:**
- **SSH**, and the **SFTP** browser, **tunnels**, and **X11 forwarding** that ride on it —
  these need a real `sshd`
- **Telnet, Mosh, RSH, Rlogin, XDMCP** — need a reachable server
- **Serial** — needs a real serial device
- **RDP / VNC** — need a remote desktop / VNC host
- **Remote monitor** — needs an SSH host to sample

**No artificial limits.** Unlike MobaXterm Home, macXterm places **no cap** on the number
of saved sessions, tunnels, or macros, and no cap on MultiExec panes or the built-in
servers. It's MIT-licensed and free to use without restriction.

---

*Happy hacking — macXterm gives you one window for shells, remote sessions, files, tunnels,
and network tools, on whichever OS you're on.*
