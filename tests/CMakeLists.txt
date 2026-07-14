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
)

foreach(t ${MACXTERM_TESTS})
  add_executable(${t} ${t}.cpp)
  target_link_libraries(${t} PRIVATE macxterm_core Qt6::Test)
  add_test(NAME ${t} COMMAND ${t})
endforeach()
