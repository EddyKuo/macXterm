[English](BUILD.md) | **中文**

# 建置 macXterm

macXterm 是一個單一程序、原生的 Qt + C/C++ 桌面應用程式。它會建置成一個執行檔
（`macXterm`）加上一個所有測試都會連結的 `macxterm_core` 靜態函式庫。建置流程由
**CMake（≥ 3.21）** 驅動。

## 相依套件

所有必要的相依套件皆採用寬鬆授權（與 MIT 相容）。

| 相依套件 | 用途 | 授權 | 是否必要 |
|------------|---------|---------|----------|
| Qt 6（Core, Gui, Widgets, Network, SerialPort, Sql, Test） | UI、事件迴圈、Socket、序列埠、SQLite、測試 | LGPLv3（**僅限動態連結**） | ✅ |
| OpenSSL 3 | AES-256-GCM + Argon2id/scrypt（憑證保險庫） | Apache-2.0 | ✅ |
| libvterm | VT100/VT220/xterm 終端機模擬 | MIT | ✅ |
| libssh2 | SSH / SFTP / 通道（tunnels） | BSD-3 | ✅ |
| zlib | VNC ZRLE inflate | zlib | ✅（通常由系統提供） |
| FreeRDP 3 | 真實 RDP 協定（自動偵測） | Apache-2.0 | 選用 |
| libpcap | 封包擷取（自動偵測） | BSD | 選用 |

> **附註。** Argon2id 來自 OpenSSL 3.2+ 的 `EVP_KDF`——**沒有**獨立的 `argon2`
> 相依套件，較舊版 OpenSSL（3.0/3.1，例如 Ubuntu 24.04）會自動回退使用
> scrypt。Qt 採**動態連結**，以便在 MIT 專案中滿足 LGPL 要求——若無商業授權，
> **請勿**靜態連結 Qt。若找到 FreeRDP，RDP 會以真實協定支援建置
> （`MACXTERM_HAVE_FREERDP`）；否則 RDP 會以骨架（scaffold）形式建置。

### macOS（Homebrew）

```sh
brew install qt openssl@3 libvterm libssh2 zlib cmake
brew install freerdp        # 選用 — 啟用真實 RDP
```
（zlib 隨 macOS 附帶；只有在 CMake 找不到系統版本時才需要 Homebrew 的
formula。）

### Linux（Debian / Ubuntu）

```sh
sudo apt install qt6-base-dev libqt6serialport6-dev libssl-dev \
                 libvterm-dev libssh2-1-dev zlib1g-dev cmake ninja-build
sudo apt install libfreerdp-dev libpcap-dev   # 選用 — 真實 RDP / 封包擷取
```

### Windows

**已驗證的建置流程（MSVC 2022 + Qt 6.8 + vcpkg）。** 這套流程在今日能成功建置、
連結並啟動 `macXterm.exe`；功能對等的路線圖請參閱
[WINDOWS_SPRINT.md](WINDOWS_SPRINT.md)（SSH 在 Windows 上目前仍為 stub——最高
優先的 W2.1）。

1. **工具鏈：** Visual Studio 2022 **Build Tools**，安裝 *Desktop C++*
   工作負載（MSVC 14.4x + Windows 10/11 SDK）、CMake 與 Ninja。
2. **Qt：** 安裝 Qt 6.8 `msvc2022_64`（SerialPort 附加元件為選用——若缺少，建置
   會停用 Serial 工作階段）。
3. **vcpkg 相依套件：** `openssl`、`libssh2[core,openssl,zlib]`、`zlib`、`pkgconf`
   （triplet 為 `x64-windows`）。請**在 `vcvars64.bat` shell 內**執行 vcpkg，讓它
   使用該 Build Tools 實例——空的 `Professional`/`Enterprise` shell 會干擾其自動
   偵測。
4. **libvterm 不在 vcpkg 中**——需自行 vendor。neovim fork 的 `src/*.c`（9 個檔案，
   附帶預先產生的 `.inc` 編碼表）可直接以 MSVC 編譯成靜態的 `vterm.lib`；透過
   `-DVTERM_LIB=` / `-DVTERM_INCLUDE=` 傳入其路徑。
5. **libssh（具備伺服器能力）為選用**——啟用內嵌的 SSH/SFTP 伺服器。其 vcpkg port
   會從 `libssh.org` 下載，而該站點並非總是可連線；請改以 `scripts/win/build-libssh.ps1`
   從 GitHub 鏡像 vendor，並傳入 `-DSSH_LIB=` / `-DSSH_INCLUDE=`。省略這些參數則會在
   不含內嵌伺服器的情況下建置。

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

ConPTY（`src/platform/Pty.cpp`）需要 Windows 10（RS5）SDK 標頭檔，原始碼會自動
設定好。本機 shell 預設為 `%ComSpec%`（cmd.exe），並在 `%USERPROFILE%` 中啟動。

## 設定、建置、測試

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
( cd build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure )
```

`QT_QPA_PLATFORM=offscreen` 讓連結 GUI 的測試可以在無頭（headless）環境下執行
（CI 與伺服器）。整套測試在 macOS 與 Linux 上應該要 **100% 全綠**。

便利腳本：

```sh
scripts/build.sh          # 設定 + 建置 + 測試（Unix）
scripts/build.ps1         # 相同功能，Windows PowerShell
scripts/linux-build.sh    # 在 Ubuntu 容器內建置 + 測試（需要 Docker）
```

## CMake 選項

| 選項 | 預設值 | 意義 |
|--------|---------|---------|
| `MACXTERM_BUILD_GUI` | ON | 建置 `macXterm` GUI 執行檔 |
| `MACXTERM_BUILD_TESTS` | ON | 建置 CTest 測試套件 |
| `MACXTERM_HAVE_FREERDP` | 自動 | 偵測到 FreeRDP 時定義 → 真實 RDP |

## 即時端對端測試裝置（選用）

部分協定測試屬於「guarded-live」：當有設定真實伺服器時會針對其執行，否則會
`QSKIP`。輔助腳本會部署可拋棄式測試裝置：

```sh
scripts/live-sshd.sh      # 部署本機 sshd 並執行即時 SSH 端對端測試
scripts/rdp-fixture.sh    # 部署 sfreerdp-server 並執行即時 RDP 端對端測試
```

## 執行應用程式

```sh
./build/src/macXterm
```

主視窗會開啟，包含一個工作階段樹側邊欄、一個分頁式終端機區域、一個工具列
（New Shell / New Session / MultiExec / Tunnel / Settings / Vault），以及一個
初始的本機 shell 分頁。

## 專案結構

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
docs/         使用者與開發者文件
scripts/      建置 + 即時測試裝置輔助工具
```

如需完整的技術設計，請參閱 [DESIGN.zh.md](DESIGN.zh.md)。
