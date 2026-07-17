**English** | [中文](USER_GUIDE.zh.md)

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
- A set of built-in **network tools**: port scanner, subnet sweep, packet capture,
  key generation, image viewer, text/folder diff, a remote system monitor, and light
  TFTP / HTTP / FTP / Telnet / CRON / NFS / SSH servers.
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
  A **filter box** at the top narrows the tree as you type, matching a session's name,
  host, username, or folder (case-insensitive); folders with no surviving match drop out,
  and clearing the box restores the full tree.
  **Right-click** any row for a context menu of editing actions: on a **session** —
  Open, Open SFTP browser (SSH/SFTP), Edit, Rename, Duplicate, Set icon, Move to folder
  (with an inline *New folder…*), Copy host address / Copy SSH command, and Delete; on a
  **folder** — New session here, Rename folder, Remove folder (keeping its sessions), and
  Expand/Collapse all; on the **root** — New session and Expand/Collapse all.
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

The New Session dialog offers **14 types**: SSH, Telnet, Serial, Mosh, RSH, Rlogin,
XDMCP, SFTP, FTP, S3, RDP, VNC, Browser, and a local Shell. The dialog is organized into
**General / Advanced / Terminal** tabs, and only the fields relevant to the chosen type
are shown.

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
- **SFTP** — a dedicated SFTP file-browser session (no terminal). Needs **Host**/**Username**.
- **FTP** — a graphical FTP browser (passive mode). Needs **Host**.
- **S3** — an Amazon S3 bucket browser (SigV4-signed). Needs the bucket and credentials.
- **RDP** — Remote Desktop to Windows hosts. Needs **Host** (and **Username**); default port 3389.
  The **Advanced** tab exposes resolution, clipboard/drive/audio redirection, and NLA.
- **VNC** — remote framebuffer / screen sharing. Needs **Host**; default port 5900. Fully
  interactive (mouse + keyboard), with a **view-only** toggle in the Advanced tab.
- **Browser** — an embedded web view with an address bar.
- **Shell** — a **local** command shell on your own machine. No host/username needed; this
  is what the startup tab uses and what **New Shell** opens.

SSH's **Advanced** tab adds compression, X11 forwarding, agent auth/forwarding, a
**keepalive** interval, a **remote command** to run instead of a login shell, and
"keep the pane open after the command exits". Every session can also be filed into a
**folder** and given a display **icon** (General tab), and terminal sessions can override
the global font/scheme/scrollback/Backspace per bookmark (**Terminal** tab).

---

## 4. Using the terminal

Each session opens in its own **tab**. Tabs are **closable** (the × on the tab, or
`Ctrl+Shift+W`) and **movable** (drag to reorder). Switch tabs with `Ctrl+Tab` /
`Ctrl+Shift+Tab`.

### Emulation

The terminal is driven by a real **VT100/VT220/xterm** emulator (built on libvterm), so
full-screen curses apps — `vim`, `htop`, `tmux`, `less`, and friends — render correctly,
including 256-color **and true-color**, cursor movement, and device-status replies.
Emoji and other astral-plane glyphs display intact, and you can **type CJK (Chinese /
Japanese / Korean) via your input method** in both local and remote sessions.

### More terminal features

- **Split panes** — split a tab 2 vertical, 2 horizontal, or 2×2, from the View menu.
- **Right-click menu** — copy / paste / select-all / clear scrollback / find.
- **Find in scrollback** — `Cmd`/`Ctrl+Shift+F` opens a search bar over the history.
- **Open URLs** — `Cmd`/`Ctrl`-click a link in the output to open it in your browser.
- **Mouse reporting** — apps that request it (tmux, vim, htop) receive mouse events.
- **Bracketed paste** and an optional **paste delay** for slow remotes; a multi-line paste
  warns before sending.
- **Session logging** — mirror a session's output to a file.

### Color schemes

Three schemes ship in the box, selectable in **Settings → Terminal → Color scheme**:

- **Dark** (default) — light-grey text on black, standard xterm 16-color ANSI palette.
- **Light** — black text on white.
- **Solarized Dark** — the familiar Solarized background/foreground with tuned base colors.

### Fonts

Set the **Font** family and **Font size** (6–72 pt) in **Settings → Terminal**. You can
also configure the **Scrollback** buffer (number of lines retained, up to 1,000,000).
Powerline / Nerd Font prompt glyphs fall back automatically to an installed Nerd Font, so
themed shell prompts render without tofu boxes. Any of these can be **overridden per
session** in the New Session dialog's **Terminal** tab.

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

## 6. SFTP & FTP file browsers

Every SSH session opens a graphical **SFTP** side panel over the same authenticated
connection — no second login — and FTP sessions get the same panel over FTP. The browser
docks on the left (matching MobaXterm) and lets you:

- **Browse** remote directories with a Name / Size / **Modified** column and sortable
  headers; jump to **Home** or up a level.
- **Download / upload** by button *or* **drag-and-drop** — drop OS files onto the panel to
  upload, or drag a remote file out to download it. Whole folders transfer **recursively**
  with a cancelable progress bar.
- **Edit remote files in place** — double-click a file to open it in the built-in editor;
  saving **re-uploads automatically**.
- **Manage** — right-click for chmod / rename / delete / new folder.
- **Follow the terminal's folder** — when enabled, the browser re-homes to whatever
  directory the terminal is currently in (via OSC 7).

The panel **closes automatically when its session's tab is closed**. Live browsing
requires a real remote endpoint; the path-handling, listing, and recursive-transfer logic
are independently unit-tested (SFTP against a real `sshd`; FTP against an embedded server).

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
- **Subnet sweep** — ping/probe a CIDR range to find live hosts.
- **Packet capture** — a `libpcap`-backed capture with a built-in packet decoder (needs
  capture permission on the interface).
- **Key generation** — generate and fingerprint SSH keys (MobaKeyGen-style).
- **Image viewer** — a full-screen picture viewer with next/previous navigation.
- **Text & folder diff** — a line-level, LCS-based text diff plus a recursive folder
  comparison (MobaTextDiff / MobaFoldersDiff-style).
- **Colour-scheme editor** — a graphical ANSI-palette / foreground-background editor.
- **Light servers** (toolbar → Servers), each without MobaXterm Home's runtime cap:
  **TFTP** (RFC 1350), **HTTP GET** file server, **FTP** (RFC 959, with a full passive
  data channel), **Telnet**, **CRON** (5-field expression scheduler), **NFSv3
  (read/write)**, and an **SSH/SFTP** server (password auth + PTY shell).
- **Remote monitor** — a status bar under SSH terminals showing live CPU / RAM / NET
  parsed from the remote host.

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
- Local **Shell** tabs, VT100/xterm emulation (true-color, emoji, CJK input), split panes
- Colour schemes, fonts (with Nerd Font fallback), scrollback, per-session overrides, Settings
- The **credential vault** (AES-256-GCM + Argon2id/scrypt) and folder/icon session tree
- **MultiExec** broadcast; **macros** and **shortcuts**
- **Text/folder diff**, **image viewer**, **port scanner**, **subnet sweep**, and the light
  **TFTP / HTTP / FTP / Telnet / CRON / NFS / SSH** servers
- OpenSSH `~/.ssh/config` and `MobaXterm.ini` **import**
- Tunnel and session **validation** (port-collision and field checks)

**Needs a live remote (or hardware) to exercise end to end:**
- **SSH**, and the **SFTP** browser, **tunnels**, and **X11 forwarding** that ride on it —
  these need a real `sshd`
- **Telnet, Mosh, RSH, Rlogin** — need a reachable server
- **FTP / S3** — need a reachable FTP server / S3 bucket
- **Serial** — needs a real serial device
- **RDP / VNC** — need a remote desktop / VNC host
- **Remote monitor** — needs an SSH host to sample
- **Packet capture** — needs capture permission on a live interface

**Known gaps.** **XDMCP** completes the discovery/negotiation handshake but does not yet
redirect the accepted session to a local X server (needs a real display manager);
**VNC** decodes Raw/CopyRect/RRE/Hextile/ZRLE (Tight is not yet implemented); and on
**Windows** the ConPTY local shell is incomplete. Windows-only MobaXterm features (WSL,
Cygwin shell extensions, MobApt, PuTTY-registry import) are intentionally out of scope on
macOS/Linux by design.

**No artificial limits.** Unlike MobaXterm Home, macXterm places **no cap** on the number
of saved sessions, tunnels, or macros, and no cap on MultiExec panes or the built-in
servers. It's MIT-licensed and free to use without restriction.

---

*Happy hacking — macXterm gives you one window for shells, remote sessions, files, tunnels,
and network tools, on whichever OS you're on.*
