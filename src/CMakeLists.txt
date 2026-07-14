# ── macxterm_core: all non-GUI logic (testable static library) ──
add_library(macxterm_core STATIC
  core/Session.cpp
  core/SessionTree.cpp
  core/IniStore.cpp
  core/SshConfigImporter.cpp
  core/CredentialVault.cpp
  core/Settings.cpp
  core/Macro.cpp
  core/Store.cpp
  core/ShortcutRegistry.cpp
  core/CliOptions.cpp
  core/SessionForm.cpp
  platform/Pty.cpp
  term/ScreenBuffer.cpp
  term/VtEngine.cpp
  term/ColorScheme.cpp
  connect/IConnection.cpp
  connect/LocalShellConnection.cpp
  connect/SshConnection.cpp
  connect/TelnetProtocol.cpp
  connect/TelnetConnection.cpp
  connect/SerialConnection.cpp
  connect/RfbProtocol.cpp
  connect/MoshConnection.cpp
  connect/HostKey.cpp
  connect/SimpleTcpConnection.cpp
  connect/RdpConnection.cpp
  connect/VncConnection.cpp
  tunnel/Tunnel.cpp
  tunnel/TunnelForm.cpp
  tunnel/LocalForwarder.cpp
  tunnel/SshTunnel.cpp
  tools/PortScanner.cpp
  tools/RemoteMonitor.cpp
  tools/TftpPacket.cpp
  tools/TextDiff.cpp
  tools/HttpServer.cpp
  tools/FtpCommand.cpp
  tools/FtpServer.cpp
  tools/TftpServer.cpp
  sftp/RemotePath.cpp
  sftp/SftpEntry.cpp
  sftp/SftpConnection.cpp
  x11/X11Display.cpp
)

target_include_directories(macxterm_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${OPENSSL_INCLUDE_DIR}
  ${VTERM_INCLUDE}
  ${SSH2_INCLUDE}
)

target_link_libraries(macxterm_core PUBLIC
  Qt6::Core
  Qt6::Gui
  Qt6::Network
  Qt6::SerialPort
  Qt6::Sql
  OpenSSL::Crypto
  ${VTERM_LIB}
  ${SSH2_LIB}
)

if(MACXTERM_FREERDP)
  target_compile_definitions(macxterm_core PUBLIC MACXTERM_HAVE_FREERDP)
  target_include_directories(macxterm_core PUBLIC
    ${FREERDP3_INCLUDE}
    /opt/homebrew/opt/freerdp/include/winpr3)
  target_link_libraries(macxterm_core PUBLIC
    ${FREERDP3_LIB} ${FREERDP_CLIENT3_LIB} ${WINPR3_LIB})
endif()

if(MACXTERM_BUILD_GUI)
  add_executable(macxterm
    ui/main.cpp
    ui/MainWindow.cpp
    ui/TerminalWidget.cpp
    ui/SessionDialog.cpp
    ui/VaultDialog.cpp
    ui/TunnelDialog.cpp
    ui/SettingsDialog.cpp
    ui/RdpSurfaceWidget.cpp
    ui/SftpPanel.cpp
    ui/PortScannerDialog.cpp
    ui/KeyGenDialog.cpp
    ui/TextEditorDialog.cpp
    ui/ServersDialog.cpp
  )
  target_link_libraries(macxterm PRIVATE macxterm_core Qt6::Widgets)
  set_target_properties(macxterm PROPERTIES
    OUTPUT_NAME "macXterm"
    # Windows: GUI-subsystem app so no console window opens (Qt6::EntryPoint
    # bridges WinMain -> main). macOS: a proper .app bundle so launching from
    # Finder does NOT spawn a Terminal window as its "console".
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE)
endif()
