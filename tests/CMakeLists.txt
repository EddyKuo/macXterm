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
  test_macro
  test_portscanner
  test_store
  test_colorscheme
  test_shortcuts
  test_remotepath
  test_cli
  test_rfb
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
  test_sessionform
  test_sftpentry
  test_ftp
  test_ftpclient
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
  test_serial_pty
  test_telnet_live
  test_rdp_live
  test_locale
  test_cjk_probe
  test_scrollback
  test_osc
  test_socks
  test_highlighter
  test_subnet
  test_folderdiff
  test_cronexpr
  test_sshserver_live
  test_nfs
  test_x11server
  test_truecolor
  test_packetdecode
  test_netprobe
  test_cronserver
  test_telnetserver
  test_sshserver_sftp
  test_packetcapture
  test_localshell
)

# Separate-process SSH server harness for test_sshserver_sftp (libssh server;
# the test is a libssh2 client, kept out of process to avoid a dual-crypto crash).
add_executable(sshserver_fixture fixtures/sshserver_fixture.cpp)
target_link_libraries(sshserver_fixture PRIVATE macxterm_core Qt6::Core)

foreach(t ${MACXTERM_TESTS})
  add_executable(${t} ${t}.cpp)
  target_link_libraries(${t} PRIVATE macxterm_core Qt6::Test)
  add_dependencies(${t} sshserver_fixture)
  add_test(NAME ${t} COMMAND ${t})
endforeach()
