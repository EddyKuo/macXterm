# ── Unit + integration tests, wired into ctest ──
set(MACXTERM_TESTS
  test_session
  test_ini
  test_sshconfig
  test_vault
  test_screenbuffer
  test_vtengine
  test_pty
  test_broadcaster
  test_telnet
  test_serial
  test_tunnel
  # (POSIX-only entries — test_serial_pty / test_socks / test_sshserver_live /
  #  test_sshserver_sftp — are appended below only on non-Windows platforms.)
  test_macro
  test_portscanner
  test_store
  test_colorscheme
  test_shortcuts
  test_remotepath
  test_cli
  test_rfb
  test_rfb_zrle
  test_monitor
  test_tftp
  test_mosh
  test_textdiff
  test_hostkey
  test_awssigv4
  test_s3
  test_forwarder
  test_httpserver
  test_simpletcp
  test_xdmcp
  test_sessionform
  test_sessiontree
  test_termconfig
  test_sftpentry
  test_ftp
  test_ftpclient
  test_ftpbrowser
  test_ftpserver
  test_tftpserver
  test_tunnelform
  test_x11
  test_rdp
  test_ssh_live
  test_sftp_live
  test_sshexec_live
  test_ssh_tunnel_live
  test_vnc
  test_telnet_live
  test_rdp_live
  test_locale
  test_cjk_probe
  test_scrollback
  test_osc
  test_highlighter
  test_subnet
  test_folderdiff
  test_cronexpr
  test_nfs
  test_x11server
  test_truecolor
  test_packetdecode
  test_netprobe
  test_cronserver
  test_telnetserver
  test_packetcapture
  test_localshell
  test_cygpath
  test_puttyimport
  test_winscpimport
)

# POSIX-only tests: they use <unistd.h>/<termios.h>/<sys/socket.h> etc. and drive
# the embedded servers over a POSIX transport, so they build only on Unix. On
# Windows the app itself is covered by the release/CI app build; the portable
# unit suite below still runs. (Full-suite coverage is gated on macOS/Linux.)
if(NOT WIN32)
  list(APPEND MACXTERM_TESTS
    test_serial_pty
    test_socks
    test_sshserver_live
    test_sshserver_sftp
  )
  # Separate-process SSH server harness for test_sshserver_sftp (libssh server;
  # the test is a libssh2 client, kept out of process to avoid a dual-crypto crash).
  add_executable(sshserver_fixture fixtures/sshserver_fixture.cpp)
  target_link_libraries(sshserver_fixture PRIVATE macxterm_core Qt6::Core)
endif()

# Integration tests that drive an interactive shell / PTY or bind a live network
# peer. They pass on macOS/Linux but the headless Windows CI runner has no usable
# login shell/ConPTY session or bindable peer, so their *runtime* is skipped on
# Windows. They still COMPILE on Windows (MSVC portability stays covered); only
# the ctest run is gated. (CMP0057 NEW — IN_LIST — is default at our 3.21 min.)
set(MACXTERM_WINDOWS_SKIP_TESTS
  test_pty
  test_localshell
  test_x11server
  test_telnetserver
)

foreach(t ${MACXTERM_TESTS})
  add_executable(${t} ${t}.cpp)
  target_link_libraries(${t} PRIVATE macxterm_core Qt6::Test)
  if(NOT WIN32)
    add_dependencies(${t} sshserver_fixture)
  endif()
  if(WIN32 AND (t IN_LIST MACXTERM_WINDOWS_SKIP_TESTS))
    continue()   # built for compile-coverage, but not run under ctest on Windows
  endif()
  add_test(NAME ${t} COMMAND ${t})
endforeach()
