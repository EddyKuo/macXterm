[English](README.md) | **中文**

<h1 align="center">macXterm</h1>

<p align="center">
  <b>一個原生、跨平台、MIT 授權的 <a href="https://mobaxterm.mobatek.net/">MobaXterm</a> 複製品。</b><br>
  單一行程的 Qt 6 / C++ 桌面應用程式——單一程式碼庫，沒有人為限制。
</p>

macXterm 是一個一站式遠端運算工具箱：**分頁式終端機**、**圖形化
SFTP/FTP 瀏覽器**、**SSH 隧道**、**RDP/VNC** 遠端桌面、**X11 轉發**、
**加密憑證保管庫**，以及一整套**網路工具與輕量伺服器**——
全部整合在一個原生應用程式中，支援 **macOS、Linux 與 Windows**。

> **狀態：** 建置乾淨，且**完整的 71 套測試套件**在 macOS 與 Linux 上皆通過
> （包含針對本機 fixture 的即時 SSH/RDP 端對端測試）。**完整的應用程式在
> Windows 上也可以建置、連結並啟動**（MSVC 2022 + Qt 6.8 + vcpkg）。在 Windows 上
> 已可運作的功能：**ConPTY 本機 shell**（cmd/PowerShell）、透過 Winsock 傳輸的
> **SSH／SFTP／隧道／SOCKS**、**X11 轉發**（一鍵式 VcXsrv）、**WSL session**、
> **PuTTY／WinSCP 匯入**、**DPAPI 帳號綁定保管庫**、**NFS 伺服器**、**內嵌
> SSH/SFTP 伺服器**、`cygpath`／`/drives` 對應，以及 Windows shell 整合。剩餘的
> 項目僅需外部產物——一個內附的 BusyBox 二進位檔與一張 Authenticode 簽章
> 憑證——並追蹤於 **[`docs/WINDOWS_SPRINT.md`](docs/WINDOWS_SPRINT.md)**。詳見
> [Feature coverage](#feature-coverage) 與 [`docs/DESIGN.zh.md`](docs/DESIGN.zh.md) §20。

---

## 亮點

- **14 種 session 類型** — SSH、Telnet、RSH、Rlogin、Serial、Mosh、SFTP、FTP、Amazon S3、本機 Shell、RDP、VNC、XDMCP，以及內嵌瀏覽器。
- **功能完整的終端機** — 透過 `libvterm` 支援 VT100/VT220/xterm、256 色**與真彩色**、emoji／星空平面字符、**CJK 輸入法（IME）輸入**、語法高亮、session 紀錄、括號貼上模式、滑鼠回報（1000/1002/1003 + SGR 1006）、捲動歷史搜尋，以及 `Ctrl`/`Cmd`-click 開啟網址。
- **每個 session 可個別覆寫** — 字型、色彩配置、捲動歷史與 Backspace 編碼可全域設定，也可依書籤個別設定。
- **分頁與窗格** — 拖曳重新排序、分離／重新附加為浮動視窗、2/2/4 分割窗格，以及 **MultiExec**（將一個按鍵廣播到所有可見窗格）。
- **圖形化 SFTP 與 FTP 瀏覽器** — 拖放傳輸、跟隨終端機當前目錄、雙擊**遠端編輯並自動重新儲存**、可取消進度條的遞迴資料夾傳輸。
- **SSH 生態系統** — 跳板主機、代理驗證、X11 轉發、壓縮、保持連線、執行遠端指令，以及本機／遠端／**動態（SOCKS）**隧道（RDP/VNC 也可透過跳板主機路由）。
- **遠端桌面** — 透過 FreeRDP 的 **RDP**（解析度、剪貼簿／磁碟機／音訊重新導向、NLA）與從零打造、符合 MIT 授權的 RFB 客戶端的 **VNC**，支援 **Raw / CopyRect / RRE / Hextile / ZRLE** 解碼——兩者皆完全可互動（滑鼠＋鍵盤），並提供唯讀模式。
- **有組織的 session 樹** — 資料夾＋每個書籤的圖示、**即時篩選框**（名稱／主機／使用者／資料夾）、**右鍵選單**（編輯／重新命名／複製／移動到資料夾／設定圖示／複製 SSH 指令／刪除）、加密保管庫，以及從 OpenSSH `~/.ssh/config` 與 `MobaXterm.ini` 匯入。
- **內建輕量伺服器** — TFTP、HTTP、FTP、Telnet、CRON、**NFSv3（讀寫）**，以及 SSH/SFTP——從工具列啟動，無執行期上限。
- **網路工具箱** — 連接埠掃描器、子網路／CIDR 掃描、封包擷取（`libpcap`）、SSH 金鑰產生、圖片檢視器、文字與資料夾差異比對，以及即時遠端 CPU/RAM/NET 監控列。

## 螢幕截圖／圖示

macXterm 內附一個友善的終端機吉祥物應用程式圖示
（[`assets/icon/`](assets/icon/)）。整個使用者介面——包括 macOS 上的選單列——
都位於主視窗之中，與 MobaXterm 的單一視窗版面一致。

## 快速開始

macXterm 使用 CMake 搭配 Qt 6、OpenSSL 3、`libvterm`、`libssh2` 與 `zlib`
建置（FreeRDP 為選用項目，可啟用真正的 RDP）。各平台的完整安裝說明請見
**[`docs/BUILD.zh.md`](docs/BUILD.zh.md)**。

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

## 文件

| 文件 | 對象 |
|----------|----------|
| **[docs/MANUAL.zh.md](docs/MANUAL.zh.md)** | **完整圖解使用說明書** — 含 Windows/PowerShell/WSL 全功能、截圖、疑難排解 |
| **[docs/USER_GUIDE.zh.md](docs/USER_GUIDE.zh.md)** | 使用 macXterm — session、終端機、SFTP/FTP、隧道、保管庫、工具 |
| **[docs/BUILD.zh.md](docs/BUILD.zh.md)** | 在 macOS／Linux／Windows 上從原始碼建置 |
| **[docs/DESIGN.zh.md](docs/DESIGN.zh.md)** | 技術設計 — 架構、模組、協定、資料模型、執行緒、安全性 |

## Feature coverage

macXterm 涵蓋了 MobaXterm 完整的跨平台功能範圍，而僅限 Windows 的功能現在
正被推向 100% 對等，而非略過：

- **Windows 對等——已實作**（詳見 [`docs/WINDOWS_SPRINT.md`](docs/WINDOWS_SPRINT.md)）：
  ConPTY 本機 shell（附 cmd/PowerShell/pwsh 選擇器）、透過 Winsock 傳輸的
  SSH／SFTP／隧道／SOCKS、X11 轉發（一鍵式 VcXsrv）、WSL session、PuTTY 與
  WinSCP 匯入、DPAPI 保管庫綁定、NFS 與內嵌 SSH/SFTP 伺服器、`cygpath`／`/drives`
  路徑對應搭配本機 Unix 終端機啟動器，以及 Windows shell 整合皆已可運作。
  剩餘的項目僅為外部產物——一個內附的 BusyBox 二進位檔與一張 Authenticode
  簽章憑證。MobApt 套件管理器則予以延後。
- **需要外部基礎設施才能完成**：XDMCP 交握後的 X-display 重新導向
  （需要真正的顯示管理器），以及一個*內附*的 X.Org 伺服器（macXterm 改為啟動
  平台自身的 X 伺服器——XQuartz／VcXsrv）。

## 專案結構

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

## 技術與授權

macXterm 採用 **MIT 授權**，且只連結寬鬆授權的相依套件：Qt 6
（LGPL，動態連結）、OpenSSL（Apache-2.0）、`libvterm`（MIT）、`libssh2`（BSD）、
`zlib`，以及 FreeRDP（Apache-2.0）。VNC 是從零打造的 RFB 實作，Mosh 則是
以獨立行程的方式呼叫，因此**沒有連結任何 GPL 程式碼**。詳見
[`docs/DESIGN.zh.md`](docs/DESIGN.zh.md) §18（授權）與 §19（設計決策）。

## 貢獻

macXterm 以單一程式碼庫鎖定三個平台；變更應保持 macOS 與
Linux 的測試矩陣全綠（`ctest --test-dir build`）。非 GUI 邏輯位於
`macxterm_core` 函式庫中，並有單元測試涵蓋；平台差異則隱藏在
`src/platform/` 的薄抽象層之後。架構與慣例請見 [`docs/DESIGN.zh.md`](docs/DESIGN.zh.md)。

## 授權條款

MIT — 詳見 [LICENSE](LICENSE)。
